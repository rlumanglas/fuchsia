// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    crate::{
        errors::FxfsError,
        object_handle::{
            GetProperties, ObjectHandle, ObjectProperties, ReadObjectHandle, WriteObjectHandle,
        },
        object_store::{
            allocator::{self},
            data_buffer::{DataBufferFactory, NativeDataBuffer},
            record::Timestamp,
            store_object_handle::{round_down, round_up},
            transaction::{LockKey, Options, TRANSACTION_METADATA_MAX_AMOUNT},
            writeback_cache::{ReadResult, StorageReservation, WriteResult, WritebackCache},
            AssocObj, Mutation, ObjectKey, ObjectStore, ObjectValue, StoreObjectHandle,
        },
    },
    anyhow::{anyhow, bail, Context, Error},
    async_trait::async_trait,
    std::{
        ops::Range,
        sync::{
            atomic::{self},
            Arc,
        },
    },
    storage_device::buffer::{Buffer, BufferRef, MutableBufferRef},
};

pub use crate::object_store::writeback_cache::CACHE_READ_AHEAD_SIZE;

// TODO(jfsulliv): We should move away from having CachingObjectHandle implement ObjectHandle, since
// it stubs out most of the methods.
pub struct CachingObjectHandle<S: AsRef<ObjectStore> + Send + Sync + 'static> {
    handle: StoreObjectHandle<S>,
    cache: WritebackCache,
}

impl<S: AsRef<ObjectStore> + DataBufferFactory + Send + Sync + 'static> CachingObjectHandle<S> {
    pub fn new(handle: StoreObjectHandle<S>) -> Self {
        let size = handle.get_size();
        let buffer = handle.owner().create_data_buffer(handle.object_id(), size);
        Self { handle, cache: WritebackCache::new(buffer) }
    }
}

impl<S: AsRef<ObjectStore> + Send + Sync + 'static> CachingObjectHandle<S> {
    pub async fn read_cached(&self, offset: u64, buffer: &mut [u8]) -> Result<usize, Error> {
        loop {
            let result = self.cache.read(offset, buffer, self.block_size() as u64);
            match result {
                ReadResult::Done(bytes) => {
                    return Ok(bytes as usize);
                }
                ReadResult::MissingRanges(missing_ranges) => {
                    let fs = self.store().filesystem();
                    let _guard = fs
                        .read_lock(&[LockKey::object_attribute(
                            self.store().store_object_id,
                            self.handle.object_id,
                            self.handle.attribute_id,
                        )])
                        .await;
                    let mut buffer = self.allocate_buffer(CACHE_READ_AHEAD_SIZE as usize);
                    for missing_range in missing_ranges {
                        let range = missing_range.range().clone();
                        let aligned_start = round_down(range.start, CACHE_READ_AHEAD_SIZE);
                        let align = (range.start - aligned_start) as usize;
                        let to_read = (range.end - aligned_start) as usize;
                        let bytes_read =
                            self.handle.read(aligned_start, buffer.subslice_mut(..to_read)).await?;
                        let len = bytes_read.saturating_sub(align);
                        if len == 0 {
                            // A zero-length read would result in an infinite loop since we'd try to
                            // read in the same range again, and indicates an inconsistency between
                            // the object size and the actual amount of stored data.
                            bail!(FxfsError::Inconsistent);
                        }
                        missing_range.populate(&buffer.as_slice()[align..align + len]);
                    }
                }
                ReadResult::Contended(event) => {
                    let _ = event.await;
                }
            }
        }
    }

    pub async fn write_or_append_cached(
        &self,
        offset: Option<u64>,
        buf: &[u8],
    ) -> Result<u64, Error> {
        let time = Timestamp::now().into();
        let bs = self.block_size() as u64;
        loop {
            let result = self.cache.write_or_append(offset, buf, bs, self, Some(time))?;
            match result {
                WriteResult::Done(size) => {
                    return Ok(size);
                }
                WriteResult::MissingRanges(missing_ranges) => {
                    let fs = self.store().filesystem();
                    let _guard = fs
                        .read_lock(&[LockKey::object_attribute(
                            self.store().store_object_id,
                            self.handle.object_id,
                            self.handle.attribute_id,
                        )])
                        .await;
                    let mut buffer = self.allocate_buffer(bs as usize);
                    for missing_range in missing_ranges {
                        let range = missing_range.range().clone();
                        assert!(range.start % bs == 0);
                        assert!(range.end - range.start <= bs);
                        let bytes_read = self
                            .handle
                            .read(
                                range.start,
                                buffer.subslice_mut(..(range.end - range.start) as usize),
                            )
                            .await?;
                        missing_range.populate(&buffer.as_slice()[..bytes_read]);
                    }
                }
                WriteResult::Contended(event) => {
                    let _ = event.await;
                }
            }
        }
    }
}

impl<S: AsRef<ObjectStore> + Send + Sync + 'static> Drop for CachingObjectHandle<S> {
    fn drop(&mut self) {
        self.cache.cleanup(self);
    }
}

impl<S: AsRef<ObjectStore> + Send + Sync + 'static> StorageReservation for CachingObjectHandle<S> {
    fn reserve_for_sync(&self) -> Result<allocator::Reservation, Error> {
        self.store()
            .allocator()
            .reserve(TRANSACTION_METADATA_MAX_AMOUNT)
            .ok_or(anyhow!(FxfsError::NoSpace))
    }
    fn reserve(&self, range: Range<u64>) -> Result<allocator::Reservation, Error> {
        let size = round_up(range.end, self.block_size()).unwrap()
            - round_down(range.start, self.block_size());
        self.store().allocator().reserve(size).ok_or(anyhow!(FxfsError::NoSpace))
    }
    fn wrap_reservation(&self, amount: u64) -> allocator::Reservation {
        allocator::Reservation::new(self.store().allocator(), amount)
    }
}

impl<S: AsRef<ObjectStore> + Send + Sync + 'static> CachingObjectHandle<S> {
    pub fn owner(&self) -> &Arc<S> {
        self.handle.owner()
    }

    pub fn store(&self) -> &ObjectStore {
        self.handle.store()
    }

    pub fn data_buffer(&self) -> &NativeDataBuffer {
        &self.cache.data_buffer()
    }
}

impl<S: AsRef<ObjectStore> + Send + Sync + 'static> ObjectHandle for CachingObjectHandle<S> {
    fn set_trace(&self, v: bool) {
        self.handle.set_trace(v);
    }

    fn object_id(&self) -> u64 {
        self.handle.object_id
    }

    fn allocate_buffer(&self, size: usize) -> Buffer<'_> {
        self.handle.allocate_buffer(size)
    }

    fn block_size(&self) -> u32 {
        self.handle.block_size()
    }

    fn get_size(&self) -> u64 {
        self.cache.content_size()
    }
}

#[async_trait]
impl<S: AsRef<ObjectStore> + Send + Sync + 'static> GetProperties for CachingObjectHandle<S> {
    async fn get_properties(&self) -> Result<ObjectProperties, Error> {
        let mut props = self.handle.get_properties().await?;
        let cached_metadata = self.cache.cached_metadata();
        props.allocated_size = props.allocated_size + cached_metadata.bytes_reserved;
        props.data_attribute_size = cached_metadata.content_size;
        props.creation_time =
            cached_metadata.creation_time.map(|t| t.into()).unwrap_or(props.creation_time);
        props.modification_time =
            cached_metadata.modification_time.map(|t| t.into()).unwrap_or(props.modification_time);
        Ok(props)
    }
}

#[async_trait]
impl<S: AsRef<ObjectStore> + Send + Sync + 'static> ReadObjectHandle for CachingObjectHandle<S> {
    async fn read(&self, offset: u64, mut buf: MutableBufferRef<'_>) -> Result<usize, Error> {
        self.read_cached(offset, buf.as_mut_slice()).await
    }
}

#[async_trait]
impl<S: AsRef<ObjectStore> + Send + Sync + 'static> WriteObjectHandle for CachingObjectHandle<S> {
    async fn write_or_append(&self, offset: Option<u64>, buf: BufferRef<'_>) -> Result<u64, Error> {
        self.write_or_append_cached(offset, buf.as_slice()).await
    }

    async fn truncate(&self, size: u64) -> Result<(), Error> {
        self.cache.resize(size, self.block_size() as u64);
        Ok(())
    }

    async fn write_timestamps<'a>(
        &'a self,
        crtime: Option<Timestamp>,
        mtime: Option<Timestamp>,
    ) -> Result<(), Error> {
        self.cache.update_timestamps(crtime.map(|t| t.into()), mtime.map(|t| t.into()));
        Ok(())
    }

    async fn flush(&self) -> Result<(), Error> {
        let bs = self.block_size() as u64;
        let fs = self.store().filesystem();
        let locks = fs
            .transaction_lock(&[LockKey::object_attribute(
                self.store().store_object_id,
                self.handle.object_id,
                self.handle.attribute_id,
            )])
            .await;
        // TODO(jfsulliv): Rely on the transaction locks for serializing flush.  This requires some
        // refactoring to Transaction::commit_with_callback so that we can make the end the flush
        // with the locks still held (there are lifetime issues preventing that currently).
        let flushable = loop {
            match self.cache.take_flushable_data(bs, |size| self.allocate_buffer(size), self) {
                Ok(f) => break f,
                Err(event) => {
                    let _ = event.await;
                }
            }
        };
        if self.handle.trace.load(atomic::Ordering::Relaxed) {
            log::info!(
                "{}.{} F {:?}",
                self.store().store_object_id,
                self.handle.object_id,
                flushable
            );
        }
        if !flushable.has_data() {
            // If there's no data to flush, we just need to flush the metadata.
            if !flushable.has_metadata() {
                return Ok(());
            }
            let mut transaction = fs
                .clone()
                .new_transaction_with_locks(
                    locks,
                    Options {
                        skip_journal_checks: self.handle.options.skip_journal_checks,
                        borrow_metadata_space: true,
                        ..Default::default()
                    },
                )
                .await?;
            if flushable.metadata.content_size_changed {
                self.handle.truncate(&mut transaction, flushable.metadata.content_size).await?;
            }
            self.handle
                .write_timestamps(
                    &mut transaction,
                    flushable.metadata.creation_time.map(|t| t.into()),
                    flushable.metadata.modification_time.map(|t| t.into()),
                )
                .await?;
            transaction.commit().await?;
            return Ok(());
        }

        // TODO(jfsulliv): Limit the number of pending writes so that we don't overflow
        // the transaction.
        let mut transaction = fs
            .clone()
            .new_transaction_with_locks(
                locks,
                Options {
                    skip_journal_checks: self.handle.options.skip_journal_checks,
                    allocator_reservation: Some(flushable.reservation()),
                    ..Default::default()
                },
            )
            .await?;
        for range in flushable.ranges() {
            self.handle
                .txn_write(&mut transaction, range.offset(), range.data())
                .await
                .context("Write failed")?;
        }
        self.handle
            .write_timestamps(
                &mut transaction,
                flushable.metadata.creation_time.map(|t| t.into()),
                flushable.metadata.modification_time.map(|t| t.into()),
            )
            .await?;
        if flushable.metadata.content_size_changed {
            // TODO(jfsulliv): We also need to zero the last block here, but this is tricky to deal
            // with, since a txn_write might conflict with the write of the last block.
            let old_size = self.handle.get_size();
            let size = flushable.metadata.content_size;
            if size < old_size {
                let aligned_size = round_up(size, bs).ok_or(FxfsError::TooBig)?;
                self.handle
                    .zero(
                        &mut transaction,
                        aligned_size..round_up(old_size, bs).ok_or(FxfsError::Inconsistent)?,
                    )
                    .await?;
            }
            transaction.add_with_object(
                self.store().store_object_id,
                Mutation::replace_or_insert_object(
                    ObjectKey::attribute(self.handle.object_id, self.handle.attribute_id),
                    ObjectValue::attribute(size),
                ),
                AssocObj::Borrowed(&self.handle),
            );
        }
        transaction.commit().await.context("Failed to commit transaction")?;

        self.cache.complete_flush(flushable);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use {
        super::CACHE_READ_AHEAD_SIZE,
        crate::{
            lsm_tree::types::{ItemRef, LayerIterator},
            object_handle::{GetProperties, ObjectHandle, ReadObjectHandle, WriteObjectHandle},
            object_store::{
                crypt::InsecureCrypt,
                filesystem::{Filesystem, FxFilesystem, OpenFxFilesystem},
                record::{ExtentKey, ObjectKey, ObjectValue, Timestamp},
                transaction::{Options, TransactionHandler},
                CachingObjectHandle, HandleOptions, ObjectStore,
            },
        },
        fuchsia_async as fasync,
        rand::Rng,
        std::ops::Bound,
        std::sync::Arc,
        std::time::Duration,
        storage_device::{fake_device::FakeDevice, DeviceHolder},
    };

    const TEST_DEVICE_BLOCK_SIZE: u32 = 512;

    // Some tests (the preallocate_range ones) currently assume that the data only occupies a single
    // device block.
    const TEST_DATA_OFFSET: u64 = 5000;
    const TEST_DATA: &[u8] = b"hello";
    const TEST_OBJECT_SIZE: u64 = 5678;

    async fn test_filesystem() -> OpenFxFilesystem {
        let device = DeviceHolder::new(FakeDevice::new(8192, TEST_DEVICE_BLOCK_SIZE));
        FxFilesystem::new_empty(device, Arc::new(InsecureCrypt::new()))
            .await
            .expect("new_empty failed")
    }

    async fn test_filesystem_and_object() -> (OpenFxFilesystem, CachingObjectHandle<ObjectStore>) {
        let fs = test_filesystem().await;
        let store = fs.root_store();
        let handle;
        let mut transaction = fs
            .clone()
            .new_transaction(&[], Options::default())
            .await
            .expect("new_transaction failed");
        handle =
            ObjectStore::create_object(&store, &mut transaction, HandleOptions::default(), Some(0))
                .await
                .expect("create_object failed");
        transaction.commit().await.expect("commit failed");
        let object = CachingObjectHandle::new(handle);
        {
            let align = TEST_DATA_OFFSET as usize % TEST_DEVICE_BLOCK_SIZE as usize;
            let mut buf = object.allocate_buffer(align + TEST_DATA.len());
            buf.as_mut_slice()[align..].copy_from_slice(TEST_DATA);
            object
                .write_or_append(Some(TEST_DATA_OFFSET), buf.subslice(align..))
                .await
                .expect("write failed");
        }
        object.truncate(TEST_OBJECT_SIZE).await.expect("truncate failed");
        object.flush().await.expect("flush failed");
        (fs, object)
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_zero_buf_len_read() {
        let (fs, object) = test_filesystem_and_object().await;
        let mut buf = object.allocate_buffer(0);
        assert_eq!(object.read(0u64, buf.as_mut()).await.expect("read failed"), 0);
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_beyond_eof_read() {
        let (fs, object) = test_filesystem_and_object().await;
        let offset = TEST_OBJECT_SIZE as usize - 2;
        let align = offset % fs.block_size() as usize;
        let len: usize = 2;
        let mut buf = object.allocate_buffer(align + len + 1);
        buf.as_mut_slice().fill(123u8);
        assert_eq!(
            object.read((offset - align) as u64, buf.as_mut()).await.expect("read failed"),
            align + len
        );
        assert_eq!(&buf.as_slice()[align..align + len], &vec![0u8; len]);
        assert_eq!(&buf.as_slice()[align + len..], &vec![123u8; buf.len() - align - len]);
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_read_sparse() {
        let (fs, object) = test_filesystem_and_object().await;
        // Deliberately read not right to eof.
        let len = TEST_OBJECT_SIZE as usize - 1;
        let mut buf = object.allocate_buffer(len);
        buf.as_mut_slice().fill(123u8);
        assert_eq!(object.read(0, buf.as_mut()).await.expect("read failed"), len);
        let mut expected = vec![0; len];
        let offset = TEST_DATA_OFFSET as usize;
        expected[offset..offset + TEST_DATA.len()].copy_from_slice(TEST_DATA);
        assert_eq!(buf.as_slice()[..len], expected[..]);
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_read_after_writes_interspersed_with_flush() {
        let fs = test_filesystem().await;
        let mut transaction = fs
            .clone()
            .new_transaction(&[], Options::default())
            .await
            .expect("new_transaction failed");
        let handle = ObjectStore::create_object(
            &fs.root_store(),
            &mut transaction,
            HandleOptions::default(),
            Some(0),
        )
        .await
        .expect("create_object failed");
        transaction.commit().await.expect("transaction commit failed");
        let object = CachingObjectHandle::new(handle);

        let mut buf = object.allocate_buffer(TEST_DATA.len());
        buf.as_mut_slice().copy_from_slice(TEST_DATA);
        object.write_or_append(Some(0), buf.as_ref()).await.expect("write failed");

        object.flush().await.expect("flush failed");

        let mut buf = object.allocate_buffer(TEST_DATA.len());
        buf.as_mut_slice().copy_from_slice(TEST_DATA);
        object.write_or_append(Some(100), buf.as_ref()).await.expect("write failed");

        let mut buf = object.allocate_buffer(TEST_DATA.len());
        buf.as_mut_slice().copy_from_slice(TEST_DATA);
        object.write_or_append(Some(TEST_DATA_OFFSET), buf.as_ref()).await.expect("write failed");

        let len = object.get_size() as usize;
        assert_eq!(len, TEST_DATA_OFFSET as usize + TEST_DATA.len());
        let mut buf = object.allocate_buffer(len);
        buf.as_mut_slice().fill(123u8);
        assert_eq!(object.read(0, buf.as_mut()).await.expect("read failed"), len);

        let mut expected = vec![0u8; len];
        expected[..TEST_DATA.len()].copy_from_slice(TEST_DATA);
        expected[100..100 + TEST_DATA.len()].copy_from_slice(TEST_DATA);
        expected[TEST_DATA_OFFSET as usize..TEST_DATA_OFFSET as usize + TEST_DATA.len()]
            .copy_from_slice(TEST_DATA);
        assert_eq!(buf.as_slice(), &expected);
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_read_after_truncate_and_extend() {
        let (fs, object) = test_filesystem_and_object().await;

        // Arrange for there to be <extent><deleted-extent><extent>.
        let mut buf = object.allocate_buffer(TEST_DATA.len());
        buf.as_mut_slice().copy_from_slice(TEST_DATA);
        // This adds an extent at 0..512.
        object.write_or_append(Some(0), buf.as_ref()).await.expect("write failed");
        object.truncate(3).await.expect("truncate failed"); // This deletes 512..1024.
        let data = b"foo";
        let offset = 1500u64;
        let align = (offset % fs.block_size() as u64) as usize;
        let mut buf = object.allocate_buffer(align + data.len());
        buf.as_mut_slice()[align..].copy_from_slice(data);
        object.write_or_append(Some(1500), buf.subslice(align..)).await.expect("write failed"); // This adds 1024..1536.

        const LEN1: usize = 1503;
        let mut buf = object.allocate_buffer(LEN1);
        buf.as_mut_slice().fill(123u8);
        assert_eq!(object.read(0, buf.as_mut()).await.expect("read failed"), LEN1);
        let mut expected = [0; LEN1];
        expected[..3].copy_from_slice(&TEST_DATA[..3]);
        expected[1500..].copy_from_slice(b"foo");
        assert_eq!(buf.as_slice(), &expected);

        // Also test a read that ends midway through the deleted extent.
        const LEN2: usize = 601;
        let mut buf = object.allocate_buffer(LEN2);
        buf.as_mut_slice().fill(123u8);
        assert_eq!(object.read(0, buf.as_mut()).await.expect("read failed"), LEN2);
        assert_eq!(buf.as_slice(), &expected[..LEN2]);
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_read_whole_blocks_with_multiple_objects() {
        let (fs, object) = test_filesystem_and_object().await;
        let mut buffer = object.allocate_buffer(512);
        buffer.as_mut_slice().fill(0xaf);
        object.write_or_append(Some(0), buffer.as_ref()).await.expect("write failed");

        let store = object.owner();
        let mut transaction = fs
            .clone()
            .new_transaction(&[], Options::default())
            .await
            .expect("new_transaction failed");
        let handle2 =
            ObjectStore::create_object(&store, &mut transaction, HandleOptions::default(), Some(0))
                .await
                .expect("create_object failed");
        transaction.commit().await.expect("commit failed");
        let object2 = CachingObjectHandle::new(handle2);
        let mut ef_buffer = object.allocate_buffer(512);
        ef_buffer.as_mut_slice().fill(0xef);
        object2.write_or_append(Some(0), ef_buffer.as_ref()).await.expect("write failed");

        let mut buffer = object.allocate_buffer(512);
        buffer.as_mut_slice().fill(0xaf);
        object.write_or_append(Some(512), buffer.as_ref()).await.expect("write failed");
        object.truncate(1536).await.expect("truncate failed");
        object2.write_or_append(Some(512), ef_buffer.as_ref()).await.expect("write failed");

        let mut buffer = object.allocate_buffer(2048);
        buffer.as_mut_slice().fill(123);
        assert_eq!(object.read(0, buffer.as_mut()).await.expect("read failed"), 1536);
        assert_eq!(&buffer.as_slice()[..1024], &[0xaf; 1024]);
        assert_eq!(&buffer.as_slice()[1024..1536], &[0; 512]);
        assert_eq!(object2.read(0, buffer.as_mut()).await.expect("read failed"), 1024);
        assert_eq!(&buffer.as_slice()[..1024], &[0xef; 1024]);
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_truncate_deallocates_old_extents() {
        let (fs, object) = test_filesystem_and_object().await;
        let mut buf = object.allocate_buffer(5 * fs.block_size() as usize);
        buf.as_mut_slice().fill(0xaa);
        object.write_or_append(Some(0), buf.as_ref()).await.expect("write failed");
        object.flush().await.expect("flush failed");

        let allocator = fs.allocator();
        let allocated_before_truncate = allocator.get_allocated_bytes();
        object.truncate(fs.block_size() as u64).await.expect("truncate failed");
        let allocated_before_flush = allocator.get_allocated_bytes();
        object.flush().await.expect("flush failed");
        let allocated_after = allocator.get_allocated_bytes();
        assert_eq!(allocated_before_truncate, allocated_before_flush);
        assert!(
            allocated_after < allocated_before_truncate,
            "before = {} after = {}",
            allocated_before_truncate,
            allocated_after
        );
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_adjust_refs() {
        let (fs, object) = test_filesystem_and_object().await;
        let mut transaction = fs
            .clone()
            .new_transaction(&[], Options::default())
            .await
            .expect("new_transaction failed");
        let store = object.owner();
        assert_eq!(
            store
                .adjust_refs(&mut transaction, object.object_id(), 1)
                .await
                .expect("adjust_refs failed"),
            false
        );
        transaction.commit().await.expect("commit failed");

        let allocator = fs.allocator();
        let allocated_before = allocator.get_allocated_bytes();
        let mut transaction = fs
            .clone()
            .new_transaction(&[], Options::default())
            .await
            .expect("new_transaction failed");
        assert_eq!(
            store
                .adjust_refs(&mut transaction, object.object_id(), -2)
                .await
                .expect("adjust_refs failed"),
            true
        );
        transaction.commit().await.expect("commit failed");

        assert_eq!(allocator.get_allocated_bytes(), allocated_before);

        store
            .tombstone(
                object.object_id(),
                Options { borrow_metadata_space: true, ..Default::default() },
            )
            .await
            .expect("purge failed");

        assert_eq!(allocated_before - allocator.get_allocated_bytes(), fs.block_size() as u64,);

        let layer_set = store.tree.layer_set();
        let mut merger = layer_set.merger();
        let mut iter = merger.seek(Bound::Unbounded).await.expect("seek failed");
        let mut found = false;
        while let Some(ItemRef { key: ObjectKey { object_id, .. }, value, .. }) = iter.get() {
            if *object_id == object.object_id() {
                if let ObjectValue::None = value {
                    assert!(!found);
                    found = true;
                } else {
                    assert!(false, "Unexpected item {:?}", iter.get());
                }
            }
            iter.advance().await.expect("advance failed");
        }
        assert!(found);

        // Make sure there are no extents.
        let layer_set = store.extent_tree.layer_set();
        let mut merger = layer_set.merger();
        let mut iter = merger.seek(Bound::Unbounded).await.expect("seek failed");
        while let Some(ItemRef { key: ExtentKey { object_id, .. }, value, .. }) = iter.get() {
            assert!(*object_id != object.object_id() || value.is_deleted());
            iter.advance().await.expect("advance failed");
        }

        fs.close().await.expect("Close failed");
    }

    #[fasync::run(10, test)]
    async fn test_racy_reads() {
        let fs = test_filesystem().await;
        let handle;
        let mut transaction = fs
            .clone()
            .new_transaction(&[], Options::default())
            .await
            .expect("new_transaction failed");
        let store = fs.root_store();
        handle =
            ObjectStore::create_object(&store, &mut transaction, HandleOptions::default(), Some(0))
                .await
                .expect("create_object failed");
        let object = Arc::new(CachingObjectHandle::new(handle));
        transaction.commit().await.expect("commit failed");
        for _ in 0..100 {
            let cloned_object = object.clone();
            let writer = fasync::Task::spawn(async move {
                let mut buf = cloned_object.allocate_buffer(10);
                buf.as_mut_slice().fill(123);
                cloned_object.write_or_append(Some(0), buf.as_ref()).await.expect("write failed");
                cloned_object.flush().await.expect("flush failed");
            });
            let cloned_object = object.clone();
            let reader = fasync::Task::spawn(async move {
                let wait_time = rand::thread_rng().gen_range(0, 5);
                fasync::Timer::new(Duration::from_millis(wait_time)).await;
                let mut buf = cloned_object.allocate_buffer(10);
                buf.as_mut_slice().fill(23);
                let amount = cloned_object.read(0, buf.as_mut()).await.expect("write failed");
                // If we succeed in reading data, it must include the write; i.e. if we see the size
                // change, we should see the data too.
                if amount != 0 {
                    assert_eq!(amount, 10);
                    assert_eq!(buf.as_slice(), &[123; 10]);
                }
            });
            writer.await;
            reader.await;
            object.truncate(0).await.expect("truncate failed");
            object.flush().await.expect("flush failed");
        }
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_properties() {
        let (fs, object) = test_filesystem_and_object().await;
        let crtime = Timestamp::from_nanos(1234u64);
        let mtime = Timestamp::from_nanos(5678u64);

        object
            .write_timestamps(Some(crtime.clone()), None)
            .await
            .expect("update_timestamps failed");
        let properties = object.get_properties().await.expect("get_properties failed");
        assert_eq!(properties.creation_time, crtime);
        assert_ne!(properties.modification_time, mtime);

        object.write_timestamps(None, Some(mtime.clone())).await.expect("update_timestamps failed");
        let properties = object.get_properties().await.expect("get_properties failed");
        assert_eq!(properties.creation_time, crtime);
        assert_eq!(properties.modification_time, mtime);

        // Writes should update mtime.
        let mut buf = object.allocate_buffer(5);
        buf.as_mut_slice().copy_from_slice(b"hello");
        object.write_or_append(Some(0), buf.as_ref()).await.expect("write failed");

        let properties = object.get_properties().await.expect("get_properties failed");
        assert_eq!(properties.creation_time, crtime);
        assert_ne!(properties.modification_time, mtime);
        fs.close().await.expect("Close failed");
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_cached_writes() {
        let fs = test_filesystem().await;
        let object_id = {
            let handle;
            let mut transaction = fs
                .clone()
                .new_transaction(&[], Options::default())
                .await
                .expect("new_transaction failed");
            handle = ObjectStore::create_object(
                &fs.root_store(),
                &mut transaction,
                HandleOptions::default(),
                Some(0),
            )
            .await
            .expect("create_object failed");
            transaction.commit().await.expect("transaction commit failed");
            let object = CachingObjectHandle::new(handle);

            let mut buf = object.allocate_buffer(2 * CACHE_READ_AHEAD_SIZE as usize);

            // Simple case is an aligned, non-sparse write.
            buf.as_mut_slice()[..12].copy_from_slice(b"hello, mars!");
            object.write_or_append(Some(0), buf.subslice(..12)).await.expect("write failed");
            assert_eq!(object.get_size(), 12);

            // Partially overwrite that write.
            buf.as_mut_slice()[..6].copy_from_slice(b"earth!");
            object.write_or_append(Some(7), buf.subslice(..6)).await.expect("write failed");
            assert_eq!(object.get_size(), 13);

            // Also do an unaligned, sparse appending write.
            buf.as_mut_slice().fill(0xaa);
            object
                .write_or_append(Some(2 * CACHE_READ_AHEAD_SIZE - 1), buf.as_ref())
                .await
                .expect("write failed");

            // Also do an unaligned, sparse non-appending write.
            buf.as_mut_slice().fill(0xbb);
            object.write_or_append(Some(8000), buf.subslice(..500)).await.expect("write failed");

            // Also truncate.
            object.truncate(4 * CACHE_READ_AHEAD_SIZE - 2).await.expect("truncate failed");

            object.flush().await.expect("flush failed");

            object.object_id()
        };

        fs.close().await.expect("Close failed");
        let device = fs.take_device().await;
        device.reopen();
        let fs = FxFilesystem::open(device, Arc::new(InsecureCrypt::new()))
            .await
            .expect("FS open failed");

        let handle =
            ObjectStore::open_object(&fs.root_store(), object_id, HandleOptions::default())
                .await
                .expect("open_object failed");
        let object = CachingObjectHandle::new(handle);
        assert_eq!(object.get_size(), 4 * CACHE_READ_AHEAD_SIZE - 2);
        let mut buf = object.allocate_buffer(object.get_size() as usize);
        object.read(0, buf.as_mut()).await.expect("read failed");
        assert_eq!(&buf.as_slice()[..13], b"hello, earth!");
        assert_eq!(buf.as_slice()[13..8000], [0u8; 7987]);
        assert_eq!(buf.as_slice()[8000..8500], [0xbb; 500]);
        assert_eq!(
            buf.as_slice()[8500..2 * CACHE_READ_AHEAD_SIZE as usize - 1],
            [0u8; 2 * CACHE_READ_AHEAD_SIZE as usize - 8501]
        );
        assert_eq!(
            buf.as_slice()[2 * CACHE_READ_AHEAD_SIZE as usize - 1..],
            vec![0xaa; 2 * CACHE_READ_AHEAD_SIZE as usize - 1]
        );
    }

    #[fasync::run_singlethreaded(test)]
    async fn test_cached_writes_unflushed() {
        let fs = test_filesystem().await;
        let object_id = {
            let object;
            let mut transaction = fs
                .clone()
                .new_transaction(&[], Options::default())
                .await
                .expect("new_transaction failed");
            object = ObjectStore::create_object(
                &fs.root_store(),
                &mut transaction,
                HandleOptions::default(),
                Some(0),
            )
            .await
            .expect("create_object failed");
            transaction.commit().await.expect("transaction commit failed");
            let object = CachingObjectHandle::new(object);

            let mut buf = object.allocate_buffer(2 * CACHE_READ_AHEAD_SIZE as usize + 6);

            // First, do a write and immediately flush. Only this should persist.
            buf.as_mut_slice()[..5].copy_from_slice(b"hello");
            object.write_or_append(Some(0), buf.subslice(..5)).await.expect("write failed");
            object.flush().await.expect("Flush failed");
            assert_eq!(object.get_size(), 5);

            buf.as_mut_slice()[..5].copy_from_slice(b"bye!!");
            object.write_or_append(Some(0), buf.subslice(..5)).await.expect("write failed");

            buf.as_mut_slice().fill(0xaa);
            object
                .write_or_append(Some(CACHE_READ_AHEAD_SIZE - 5), buf.as_ref())
                .await
                .expect("write failed");
            buf.as_mut_slice().fill(0xbb);
            object
                .write_or_append(Some(CACHE_READ_AHEAD_SIZE + 1), buf.as_ref())
                .await
                .expect("write failed");
            object.truncate(3 * CACHE_READ_AHEAD_SIZE + 6).await.expect("truncate failed");

            let mut buf = object.allocate_buffer(object.get_size() as usize);
            buf.as_mut_slice().fill(0x00);
            object.read(0, buf.as_mut()).await.expect("read failed");
            assert_eq!(&buf.as_slice()[..5], b"bye!!");
            assert_eq!(
                &buf.as_slice()[5..CACHE_READ_AHEAD_SIZE as usize - 5],
                vec![0u8; CACHE_READ_AHEAD_SIZE as usize - 10]
            );
            assert_eq!(
                &buf.as_slice()
                    [CACHE_READ_AHEAD_SIZE as usize - 5..CACHE_READ_AHEAD_SIZE as usize + 1],
                vec![0xaa; 6]
            );
            assert_eq!(
                &buf.as_slice()
                    [CACHE_READ_AHEAD_SIZE as usize + 1..CACHE_READ_AHEAD_SIZE as usize + 65536],
                vec![0xbb; 65535]
            );

            object.object_id()
        };

        fs.close().await.expect("Close failed");
        let device = fs.take_device().await;
        device.reopen();
        let fs = FxFilesystem::open(device, Arc::new(InsecureCrypt::new()))
            .await
            .expect("FS open failed");

        let object =
            ObjectStore::open_object(&fs.root_store(), object_id, HandleOptions::default())
                .await
                .expect("open_object failed");
        let object = CachingObjectHandle::new(object);
        assert_eq!(object.get_size(), 5);
        let mut buf = object.allocate_buffer(5);
        object.read(0, buf.subslice_mut(..5)).await.expect("read failed");
        assert_eq!(&buf.as_slice()[..5], b"hello");
    }
}
