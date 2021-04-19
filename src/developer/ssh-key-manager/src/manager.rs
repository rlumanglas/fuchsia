// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use {
    crate::keys::{KeyEntry, ParseKeyError},
    anyhow::Context,
    fidl_fuchsia_ssh::{AuthorizedKeysRequest, AuthorizedKeysRequestStream, SshAuthorizedKeyEntry},
    fuchsia_syslog::fx_log_warn,
    fuchsia_zircon as zx,
    futures::{
        lock::{Mutex, MutexGuard},
        prelude::*,
    },
    std::{
        fs::{File, OpenOptions},
        io::{BufRead, BufReader, Read, Seek, SeekFrom, Write},
        ops::DerefMut,
        path::Path,
    },
    thiserror::Error,
};

#[derive(Error, Debug)]
pub enum SshKeyManagerError {
    #[error("i/o error")]
    IoError(#[source] std::io::Error),
    #[error("Invalid Key")]
    InvalidKey(
        #[source]
        #[from]
        ParseKeyError,
    ),
    #[error("File changed while transaction was completed")]
    TransactionInvalidated,
}

/// A transaction represents an atomic set of modifications to the authorized_keys file.
pub struct Transaction<'a, T: Read + Seek + Write + Sized> {
    file: MutexGuard<'a, T>,
    start_state: Vec<KeyEntry>,
    end_state: Vec<KeyEntry>,
}

impl<'a, T: Read + Write + Seek + Sized> Transaction<'a, T> {
    async fn new(file: MutexGuard<'a, T>) -> Result<Transaction<'a, T>, SshKeyManagerError> {
        let mut txn = Transaction { file, start_state: vec![], end_state: vec![] };
        let keys = txn.load_keys().await?;
        txn.start_state = keys.clone();
        txn.end_state = keys;
        Ok(txn)
    }

    /// Load the current set of keys from the disk, silently skipping any invalid lines.
    async fn load_keys(&mut self) -> Result<Vec<KeyEntry>, SshKeyManagerError> {
        let file = self.file.deref_mut();
        file.seek(SeekFrom::Start(0)).map_err(SshKeyManagerError::IoError)?;
        let reader = BufReader::new(file);
        let mut result = vec![];
        for line in reader.lines() {
            let line = line.map_err(SshKeyManagerError::IoError)?;
            if line.starts_with('#') || line.is_empty() {
                continue;
            }

            let entry = match line.parse::<KeyEntry>() {
                Ok(entry) => entry,
                Err(e) => {
                    fx_log_warn!("Poorly formatted line in authorized_keys: {}: {}", line, e);
                    continue;
                }
            };
            result.push(entry);
        }
        Ok(result)
    }

    /// Write the final set of keys from this transaction to the disk.
    async fn write_keys(mut self) -> Result<(), std::io::Error> {
        let file = self.file.deref_mut();
        file.seek(SeekFrom::Start(0))?;

        file.write_all(
            "# This file is auto-generated by ssh-key-manager, edits may be overwritten.\n"
                .as_bytes(),
        )?;
        for entry in self.end_state.iter() {
            file.write_all(entry.to_string().as_bytes())?;
            file.write_all(b"\n")?;
        }

        Ok(())
    }

    /// Add a key to the transaction. The key is not actually added until commit() is called.
    pub fn add_key(&mut self, key: String) -> Result<(), SshKeyManagerError> {
        let key = key.parse::<KeyEntry>()?;
        self.end_state.push(key);
        Ok(())
    }

    /// Commit all the changes in this Transaction to the disk, or fail if the underlying
    /// authorized_keys file was modified since the transaction started.
    pub async fn commit(mut self) -> Result<(), SshKeyManagerError> {
        // We have to deal with the possibility that something else touched authorized_keys, even
        // if it wasn't us.
        // This isn't going to be perfect -- there's no way of guaranteeing that nothing will
        // happen between load_keys() and write_keys() below, but this is better than
        // nothing.
        let current_state = self.load_keys().await?;
        if current_state != self.start_state {
            // Refuse to write out.
            return Err(SshKeyManagerError::TransactionInvalidated);
        }

        self.write_keys().await.map_err(SshKeyManagerError::IoError)
    }
}

pub struct SshKeyManager {
    authorized_keys: Mutex<File>,
}

impl SshKeyManager {
    pub fn new(keys_path: impl AsRef<Path>) -> Result<Self, SshKeyManagerError> {
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .open(keys_path)
            .map_err(SshKeyManagerError::IoError)?;

        Ok(SshKeyManager { authorized_keys: Mutex::new(file) })
    }

    /// Begin a transaction.
    async fn begin<'a>(&'a self) -> Result<Transaction<'a, std::fs::File>, SshKeyManagerError> {
        Transaction::new(self.authorized_keys.lock().await).await
    }

    /// Add a key to the authorized keys list.
    async fn add_key(&self, key: SshAuthorizedKeyEntry) -> Result<(), SshKeyManagerError> {
        let mut txn = self.begin().await?;
        txn.add_key(key.key)?;
        txn.commit().await?;
        Ok(())
    }

    pub async fn handle_requests(
        &self,
        mut stream: AuthorizedKeysRequestStream,
    ) -> Result<(), anyhow::Error> {
        while let Some(req) = stream.try_next().await.context("Getting request")? {
            match req {
                AuthorizedKeysRequest::AddKey { key, responder } => {
                    responder.send(&mut self.add_key(key).await.map_err(|e| {
                        fx_log_warn!("Error while adding key: {:?}", e);
                        match e {
                            SshKeyManagerError::IoError(_) => zx::Status::IO.into_raw(),
                            SshKeyManagerError::InvalidKey(_) => {
                                zx::Status::INVALID_ARGS.into_raw()
                            }
                            SshKeyManagerError::TransactionInvalidated => {
                                zx::Status::INTERRUPTED_RETRY.into_raw()
                            }
                        }
                    }))?;
                }
                AuthorizedKeysRequest::WatchKeys { watcher, .. } => {
                    watcher.close_with_epitaph(zx::Status::NOT_SUPPORTED)?;
                }
                AuthorizedKeysRequest::RemoveKey { responder, .. } => {
                    responder.send(&mut Err(zx::Status::NOT_SUPPORTED.into_raw()))?;
                }
            };
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use {super::*, std::io::Cursor};

    fn make_empty_key_file() -> Mutex<Cursor<Vec<u8>>> {
        let cursor = Cursor::new(vec![]);
        Mutex::new(cursor)
    }

    fn make_key_file(content: &str) -> Mutex<Cursor<Vec<u8>>> {
        let cursor = Cursor::new(content.as_bytes().to_vec());
        Mutex::new(cursor)
    }

    fn expect_has_keys(file: Mutex<Cursor<Vec<u8>>>, expected: Vec<&str>) {
        let actual = String::from_utf8(file.into_inner().into_inner())
            .expect("valid unicode")
            .split('\n')
            .filter(|line| !line.starts_with('#') && !line.is_empty())
            .map(|line| line.to_owned())
            .collect::<Vec<String>>();

        let expected = expected.into_iter().map(|e| e.to_owned()).collect::<Vec<String>>();

        assert_eq!(actual, expected);
    }

    const TEST_KEY: &str = "ssh-rsa abcdefg";

    #[fuchsia::test]
    async fn test_add_key() {
        let file = make_empty_key_file();
        let mut txn = Transaction::new(file.lock().await).await.expect("start txn okay");
        txn.add_key(TEST_KEY.to_owned()).expect("add key ok");
        txn.commit().await.expect("commit okay");

        expect_has_keys(file, vec![TEST_KEY]);
    }

    #[fuchsia::test]
    async fn test_modify_inflight() {
        let mut file = make_empty_key_file();
        // This is OK because the place that actually holds `file` is the transaction.
        // Doing this lets us keep a mutable reference to the file, so that we can modify it after
        // the transaction reads the initial state.
        let sneaky_ref: &'static mut Cursor<Vec<u8>> =
            unsafe { std::mem::transmute(file.get_mut()) };
        let mut txn = Transaction::new(file.lock().await).await.expect("start txn okay");
        sneaky_ref.write("ssh-rsa this-is-a-new-key".as_bytes()).expect("write ok");
        txn.add_key(TEST_KEY.to_owned()).expect("add key ok");
        txn.commit().await.expect_err("commit fails");
        expect_has_keys(file, vec!["ssh-rsa this-is-a-new-key"]);
    }

    #[fuchsia::test]
    async fn test_invalid_keyfile() {
        let file = make_key_file("INVALID KEY");
        let mut txn = Transaction::new(file.lock().await).await.expect("start txn okay");
        txn.add_key(TEST_KEY.to_owned()).expect("add key ok");
        txn.commit().await.expect("commit ok");
        expect_has_keys(file, vec![TEST_KEY]);
    }
}