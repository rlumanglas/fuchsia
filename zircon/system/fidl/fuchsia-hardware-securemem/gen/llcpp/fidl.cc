// WARNING: This file is machine generated by fidlgen.

#include <fuchsia/hardware/securemem/llcpp/fidl.h>
#include <memory>

namespace llcpp {

namespace fuchsia {
namespace hardware {
namespace securemem {

namespace {

[[maybe_unused]]
constexpr uint64_t kDevice_GetSecureMemoryPhysicalAddress_Ordinal = 0x7f32703100000000lu;
[[maybe_unused]]
constexpr uint64_t kDevice_GetSecureMemoryPhysicalAddress_GenOrdinal = 0x5f37764a495847f7lu;
extern "C" const fidl_type_t fuchsia_hardware_securemem_DeviceGetSecureMemoryPhysicalAddressRequestTable;
extern "C" const fidl_type_t fuchsia_hardware_securemem_DeviceGetSecureMemoryPhysicalAddressResponseTable;

}  // namespace
template <>
Device::ResultOf::GetSecureMemoryPhysicalAddress_Impl<Device::GetSecureMemoryPhysicalAddressResponse>::GetSecureMemoryPhysicalAddress_Impl(zx::unowned_channel _client_end, ::zx::vmo secure_mem) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<GetSecureMemoryPhysicalAddressRequest, ::fidl::MessageDirection::kSending>();
  ::fidl::internal::AlignedBuffer<_kWriteAllocSize> _write_bytes_inlined;
  auto& _write_bytes_array = _write_bytes_inlined;
  uint8_t* _write_bytes = _write_bytes_array.view().data();
  memset(_write_bytes, 0, GetSecureMemoryPhysicalAddressRequest::PrimarySize);
  auto& _request = *reinterpret_cast<GetSecureMemoryPhysicalAddressRequest*>(_write_bytes);
  _request.secure_mem = std::move(secure_mem);
  ::fidl::BytePart _request_bytes(_write_bytes, _kWriteAllocSize, sizeof(GetSecureMemoryPhysicalAddressRequest));
  ::fidl::DecodedMessage<GetSecureMemoryPhysicalAddressRequest> _decoded_request(std::move(_request_bytes));
  Super::SetResult(
      Device::InPlace::GetSecureMemoryPhysicalAddress(std::move(_client_end), std::move(_decoded_request), Super::response_buffer()));
}

Device::ResultOf::GetSecureMemoryPhysicalAddress Device::SyncClient::GetSecureMemoryPhysicalAddress(::zx::vmo secure_mem) {
  return ResultOf::GetSecureMemoryPhysicalAddress(zx::unowned_channel(this->channel_), std::move(secure_mem));
}

Device::ResultOf::GetSecureMemoryPhysicalAddress Device::Call::GetSecureMemoryPhysicalAddress(zx::unowned_channel _client_end, ::zx::vmo secure_mem) {
  return ResultOf::GetSecureMemoryPhysicalAddress(std::move(_client_end), std::move(secure_mem));
}

template <>
Device::UnownedResultOf::GetSecureMemoryPhysicalAddress_Impl<Device::GetSecureMemoryPhysicalAddressResponse>::GetSecureMemoryPhysicalAddress_Impl(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::zx::vmo secure_mem, ::fidl::BytePart _response_buffer) {
  if (_request_buffer.capacity() < GetSecureMemoryPhysicalAddressRequest::PrimarySize) {
    Super::SetFailure(::fidl::DecodeResult<GetSecureMemoryPhysicalAddressResponse>(ZX_ERR_BUFFER_TOO_SMALL, ::fidl::internal::kErrorRequestBufferTooSmall));
    return;
  }
  memset(_request_buffer.data(), 0, GetSecureMemoryPhysicalAddressRequest::PrimarySize);
  auto& _request = *reinterpret_cast<GetSecureMemoryPhysicalAddressRequest*>(_request_buffer.data());
  _request.secure_mem = std::move(secure_mem);
  _request_buffer.set_actual(sizeof(GetSecureMemoryPhysicalAddressRequest));
  ::fidl::DecodedMessage<GetSecureMemoryPhysicalAddressRequest> _decoded_request(std::move(_request_buffer));
  Super::SetResult(
      Device::InPlace::GetSecureMemoryPhysicalAddress(std::move(_client_end), std::move(_decoded_request), std::move(_response_buffer)));
}

Device::UnownedResultOf::GetSecureMemoryPhysicalAddress Device::SyncClient::GetSecureMemoryPhysicalAddress(::fidl::BytePart _request_buffer, ::zx::vmo secure_mem, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::GetSecureMemoryPhysicalAddress(zx::unowned_channel(this->channel_), std::move(_request_buffer), std::move(secure_mem), std::move(_response_buffer));
}

Device::UnownedResultOf::GetSecureMemoryPhysicalAddress Device::Call::GetSecureMemoryPhysicalAddress(zx::unowned_channel _client_end, ::fidl::BytePart _request_buffer, ::zx::vmo secure_mem, ::fidl::BytePart _response_buffer) {
  return UnownedResultOf::GetSecureMemoryPhysicalAddress(std::move(_client_end), std::move(_request_buffer), std::move(secure_mem), std::move(_response_buffer));
}

::fidl::DecodeResult<Device::GetSecureMemoryPhysicalAddressResponse> Device::InPlace::GetSecureMemoryPhysicalAddress(zx::unowned_channel _client_end, ::fidl::DecodedMessage<GetSecureMemoryPhysicalAddressRequest> params, ::fidl::BytePart response_buffer) {
  params.message()->_hdr = {};
  params.message()->_hdr.ordinal = kDevice_GetSecureMemoryPhysicalAddress_Ordinal;
  auto _encode_request_result = ::fidl::Encode(std::move(params));
  if (_encode_request_result.status != ZX_OK) {
    return ::fidl::DecodeResult<Device::GetSecureMemoryPhysicalAddressResponse>::FromFailure(
        std::move(_encode_request_result));
  }
  auto _call_result = ::fidl::Call<GetSecureMemoryPhysicalAddressRequest, GetSecureMemoryPhysicalAddressResponse>(
    std::move(_client_end), std::move(_encode_request_result.message), std::move(response_buffer));
  if (_call_result.status != ZX_OK) {
    return ::fidl::DecodeResult<Device::GetSecureMemoryPhysicalAddressResponse>::FromFailure(
        std::move(_call_result));
  }
  return ::fidl::Decode(std::move(_call_result.message));
}


bool Device::TryDispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn) {
  if (msg->num_bytes < sizeof(fidl_message_header_t)) {
    zx_handle_close_many(msg->handles, msg->num_handles);
    txn->Close(ZX_ERR_INVALID_ARGS);
    return true;
  }
  fidl_message_header_t* hdr = reinterpret_cast<fidl_message_header_t*>(msg->bytes);
  switch (hdr->ordinal) {
    case kDevice_GetSecureMemoryPhysicalAddress_Ordinal:
    case kDevice_GetSecureMemoryPhysicalAddress_GenOrdinal:
    {
      auto result = ::fidl::DecodeAs<GetSecureMemoryPhysicalAddressRequest>(msg);
      if (result.status != ZX_OK) {
        txn->Close(ZX_ERR_INVALID_ARGS);
        return true;
      }
      auto message = result.message.message();
      impl->GetSecureMemoryPhysicalAddress(std::move(message->secure_mem),
        Interface::GetSecureMemoryPhysicalAddressCompleter::Sync(txn));
      return true;
    }
    default: {
      return false;
    }
  }
}

bool Device::Dispatch(Interface* impl, fidl_msg_t* msg, ::fidl::Transaction* txn) {
  bool found = TryDispatch(impl, msg, txn);
  if (!found) {
    zx_handle_close_many(msg->handles, msg->num_handles);
    txn->Close(ZX_ERR_NOT_SUPPORTED);
  }
  return found;
}


void Device::Interface::GetSecureMemoryPhysicalAddressCompleterBase::Reply(int32_t s, uint64_t paddr) {
  constexpr uint32_t _kWriteAllocSize = ::fidl::internal::ClampedMessageSize<GetSecureMemoryPhysicalAddressResponse, ::fidl::MessageDirection::kSending>();
  FIDL_ALIGNDECL uint8_t _write_bytes[_kWriteAllocSize] = {};
  auto& _response = *reinterpret_cast<GetSecureMemoryPhysicalAddressResponse*>(_write_bytes);
  _response._hdr.ordinal = kDevice_GetSecureMemoryPhysicalAddress_Ordinal;
  _response.s = std::move(s);
  _response.paddr = std::move(paddr);
  ::fidl::BytePart _response_bytes(_write_bytes, _kWriteAllocSize, sizeof(GetSecureMemoryPhysicalAddressResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<GetSecureMemoryPhysicalAddressResponse>(std::move(_response_bytes)));
}

void Device::Interface::GetSecureMemoryPhysicalAddressCompleterBase::Reply(::fidl::BytePart _buffer, int32_t s, uint64_t paddr) {
  if (_buffer.capacity() < GetSecureMemoryPhysicalAddressResponse::PrimarySize) {
    CompleterBase::Close(ZX_ERR_INTERNAL);
    return;
  }
  auto& _response = *reinterpret_cast<GetSecureMemoryPhysicalAddressResponse*>(_buffer.data());
  _response._hdr.ordinal = kDevice_GetSecureMemoryPhysicalAddress_Ordinal;
  _response.s = std::move(s);
  _response.paddr = std::move(paddr);
  _buffer.set_actual(sizeof(GetSecureMemoryPhysicalAddressResponse));
  CompleterBase::SendReply(::fidl::DecodedMessage<GetSecureMemoryPhysicalAddressResponse>(std::move(_buffer)));
}

void Device::Interface::GetSecureMemoryPhysicalAddressCompleterBase::Reply(::fidl::DecodedMessage<GetSecureMemoryPhysicalAddressResponse> params) {
  params.message()->_hdr = {};
  params.message()->_hdr.ordinal = kDevice_GetSecureMemoryPhysicalAddress_Ordinal;
  CompleterBase::SendReply(std::move(params));
}


}  // namespace securemem
}  // namespace hardware
}  // namespace fuchsia
}  // namespace llcpp
