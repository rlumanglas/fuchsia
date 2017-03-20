// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <magenta/device/devmgr.h>

#include <magenta/syscalls.h>
#include <mxio/vfs.h>
#include <mxio/debug.h>

#define MXDEBUG 0

#include "blobstore-private.h"

namespace blobstore {

void VnodeBlob::Release() {
    blobstore->ReleaseBlob(mxtl::move(blob));
    delete this;
}

mx_status_t VnodeBlob::Open(uint32_t flags) {
    if ((flags & O_DIRECTORY) && !IsDirectory()) {
        return ERR_NOT_DIR;
    }
    RefAcquire();
    return NO_ERROR;
}

mx_status_t VnodeBlob::Close() {
    RefRelease();
    return NO_ERROR;
}

ssize_t VnodeBlob::Read(void* data, size_t len, size_t off) {
    if (IsDirectory()) {
        return ERR_NOT_FILE;
    }

    size_t actual;
    mx_status_t status = blob->Read(data, len, off, &actual);
    if (status != NO_ERROR) {
        return status;
    }
    return actual;
}

ssize_t VnodeBlob::Write(const void* data, size_t len, size_t off) {
    if (IsDirectory()) {
        return ERR_NOT_FILE;
    }
    size_t actual;
    mx_status_t status = blob->Write(data, len, &actual);
    if (status != NO_ERROR) {
        return status;
    }
    return actual;
}

mx_status_t VnodeBlob::Lookup(fs::Vnode** out, const char* name, size_t len) {
    assert(memchr(name, '/', len) == nullptr);
    if ((len == 1) && (name[0] == '.') && IsDirectory()) {
        // Special case: Accessing root directory via '.'
        RefAcquire();
        *out = this;
        return NO_ERROR;
    }

    if (!IsDirectory()) {
        return ERR_NOT_SUPPORTED;
    }

    mx_status_t status;
    merkle::Digest digest;
    if ((status = digest.Parse(name, len)) != NO_ERROR) {
        return status;
    }
    VnodeBlob* vn;
    if ((status = Blobstore::LookupBlob(blobstore, digest, &vn)) < 0) {
        return status;
    }
    *out = mxtl::move(vn);
    return NO_ERROR;
}

mx_status_t VnodeBlob::Getattr(vnattr_t* a) {
    a->inode = 0;
    a->size = IsDirectory() ? 0 : blob->SizeData();
    a->mode = IsDirectory() ? V_TYPE_DIR : V_TYPE_FILE;
    a->create_time = 0;
    a->modify_time = 0;
    return NO_ERROR;
}

mx_status_t VnodeBlob::Create(fs::Vnode** out, const char* name, size_t len, uint32_t mode) {
    assert(memchr(name, '/', len) == nullptr);
    if (!IsDirectory()) {
        return ERR_NOT_SUPPORTED;
    }

    merkle::Digest digest;
    mx_status_t status;
    if ((status = digest.Parse(name, len)) != NO_ERROR) {
        return status;
    }
    VnodeBlob* vn;
    if ((status = Blobstore::NewBlob(blobstore, digest, &vn)) != NO_ERROR) {
        return status;
    }
    *out = mxtl::move(vn);
    return NO_ERROR;
}

ssize_t VnodeBlob::Ioctl(uint32_t op, const void* in_buf, size_t in_len, void* out_buf,
                         size_t out_len) {
    switch (op) {
        case IOCTL_DEVMGR_UNMOUNT_FS: {
            mx_status_t status = Sync();
            if (status != NO_ERROR) {
                error("blobstore unmount failed to sync; unmounting anyway: %d\n", status);
            }
            return blobstore->Unmount();
        }
        case IOCTL_BLOBSTORE_BLOB_INIT: {
            if (IsDirectory()) {
                return ERR_NOT_SUPPORTED;
            }

            if ((in_len != sizeof(blob_ioctl_config_t)) || (out_len != 0)) {
                return ERR_INVALID_ARGS;
            }
            const blob_ioctl_config_t* config = static_cast<const blob_ioctl_config_t*>(in_buf);
            return blob->SpaceAllocate(config->size_data);
        }
        default: {
            return ERR_NOT_SUPPORTED;
        }
    }
}

mx_status_t VnodeBlob::Unlink(const char* name, size_t len, bool must_be_dir) {
    assert(memchr(name, '/', len) == nullptr);
    if (!IsDirectory()) {
        return ERR_NOT_SUPPORTED;
    }

    mx_status_t status;
    merkle::Digest digest;
    VnodeBlob* out;
    if ((status = digest.Parse(name, len)) != NO_ERROR) {
        return status;
    } else if ((status = Blobstore::LookupBlob(blobstore, digest, &out)) < 0) {
        return status;
    }
    out->blob->QueueUnlink();
    out->RefRelease(); // We looked up the blob -- release it too.
    return NO_ERROR;
}

mx_status_t VnodeBlob::Sync() {
    // TODO(smklein): For now, this is a no-op, but it will change
    // once the kBlobFlagSync flag is in use.
    return NO_ERROR;
}

} // namespace blobstore
