#include "WubiPinyinControlProtocol.h"

#include <limits>
#include <utility>

namespace wubipinyin {
namespace {

static_assert(kControlFrameHeaderBytes ==
              sizeof(std::uint32_t) + sizeof(std::uint16_t) +
                  sizeof(std::uint16_t) + sizeof(std::uint64_t) * 3 +
                  sizeof(std::uint32_t));
static_assert(kMaxControlPayloadBytes <=
              std::numeric_limits<std::uint32_t>::max() -
                  kControlFrameHeaderBytes);

void SetError(ControlProtocolError* error, ControlProtocolError value) {
  if (error) {
    *error = value;
  }
}

bool IsKnownMessageType(ControlMessageType type) {
  switch (type) {
    case ControlMessageType::kGetSettings:
    case ControlMessageType::kUpdateSetting:
    case ControlMessageType::kListUserEntries:
    case ControlMessageType::kUpsertUserEntry:
    case ControlMessageType::kDeleteUserEntry:
    case ControlMessageType::kResetLearning:
    case ControlMessageType::kSetRoute:
    case ControlMessageType::kCommitRaw:
    case ControlMessageType::kReloadDictionaries:
    case ControlMessageType::kResponse:
    case ControlMessageType::kError:
      return true;
  }
  return false;
}

template <typename UInt>
void AppendLittleEndian(std::vector<std::uint8_t>* bytes, UInt value) {
  for (std::size_t index = 0; index < sizeof(UInt); ++index) {
    bytes->push_back(static_cast<std::uint8_t>(value & 0xff));
    value >>= 8;
  }
}

template <typename UInt>
UInt ReadLittleEndian(const std::uint8_t* bytes) {
  UInt value = 0;
  for (std::size_t index = 0; index < sizeof(UInt); ++index) {
    value |= static_cast<UInt>(bytes[index]) << (index * 8);
  }
  return value;
}

bool ValidateControlFrame(const ControlFrame& frame,
                          ControlProtocolError* error) {
  if (!IsKnownMessageType(frame.type)) {
    SetError(error, ControlProtocolError::kInvalidMessageType);
    return false;
  }
  if (frame.request_id == 0) {
    SetError(error, ControlProtocolError::kInvalidRequestId);
    return false;
  }
  if (frame.payload.size() > kMaxControlPayloadBytes) {
    SetError(error, ControlProtocolError::kPayloadTooLarge);
    return false;
  }
  return true;
}

}  // namespace

bool EncodeControlFrame(const ControlFrame& frame,
                        std::vector<std::uint8_t>* encoded,
                        ControlProtocolError* error) {
  if (!encoded) {
    SetError(error, ControlProtocolError::kInvalidArgument);
    return false;
  }
  if (!ValidateControlFrame(frame, error)) {
    return false;
  }

  std::vector<std::uint8_t> result;
  result.reserve(kControlFrameHeaderBytes + frame.payload.size());
  AppendLittleEndian(&result, kControlProtocolMagic);
  AppendLittleEndian(&result, kControlProtocolVersion);
  AppendLittleEndian(&result, static_cast<std::uint16_t>(frame.type));
  AppendLittleEndian(&result, frame.request_id);
  AppendLittleEndian(&result, frame.session_id);
  AppendLittleEndian(&result, frame.generation);
  AppendLittleEndian(&result,
                     static_cast<std::uint32_t>(frame.payload.size()));
  result.insert(result.end(), frame.payload.begin(), frame.payload.end());

  *encoded = std::move(result);
  SetError(error, ControlProtocolError::kNone);
  return true;
}

bool DecodeControlFrame(const std::uint8_t* encoded,
                        std::size_t encoded_size,
                        ControlFrame* frame,
                        ControlProtocolError* error) {
  if (!encoded || !frame) {
    SetError(error, ControlProtocolError::kInvalidArgument);
    return false;
  }
  if (encoded_size < kControlFrameHeaderBytes) {
    SetError(error, ControlProtocolError::kTruncatedHeader);
    return false;
  }

  const std::uint32_t magic = ReadLittleEndian<std::uint32_t>(encoded);
  if (magic != kControlProtocolMagic) {
    SetError(error, ControlProtocolError::kInvalidMagic);
    return false;
  }
  const std::uint16_t version =
      ReadLittleEndian<std::uint16_t>(encoded + sizeof(std::uint32_t));
  if (version != kControlProtocolVersion) {
    SetError(error, ControlProtocolError::kUnsupportedVersion);
    return false;
  }

  const std::uint16_t type_value = ReadLittleEndian<std::uint16_t>(
      encoded + sizeof(std::uint32_t) + sizeof(std::uint16_t));
  const ControlMessageType type = static_cast<ControlMessageType>(type_value);
  if (!IsKnownMessageType(type)) {
    SetError(error, ControlProtocolError::kInvalidMessageType);
    return false;
  }

  constexpr std::size_t kRequestIdOffset = 8;
  constexpr std::size_t kSessionIdOffset = 16;
  constexpr std::size_t kGenerationOffset = 24;
  const std::uint64_t request_id =
      ReadLittleEndian<std::uint64_t>(encoded + kRequestIdOffset);
  const std::uint32_t payload_size = ReadLittleEndian<std::uint32_t>(
      encoded + kControlFramePayloadLengthOffset);
  if (payload_size > kMaxControlPayloadBytes) {
    SetError(error, ControlProtocolError::kPayloadTooLarge);
    return false;
  }

  const std::size_t frame_size = kControlFrameHeaderBytes + payload_size;
  if (encoded_size < frame_size) {
    SetError(error, ControlProtocolError::kTruncatedPayload);
    return false;
  }
  if (encoded_size != frame_size) {
    SetError(error, ControlProtocolError::kTrailingBytes);
    return false;
  }

  ControlFrame decoded;
  decoded.type = type;
  decoded.request_id = request_id;
  decoded.session_id =
      ReadLittleEndian<std::uint64_t>(encoded + kSessionIdOffset);
  decoded.generation =
      ReadLittleEndian<std::uint64_t>(encoded + kGenerationOffset);
  decoded.payload.assign(encoded + kControlFrameHeaderBytes,
                         encoded + frame_size);
  if (!ValidateControlFrame(decoded, error)) {
    return false;
  }

  *frame = std::move(decoded);
  SetError(error, ControlProtocolError::kNone);
  return true;
}

bool ValidateControlRequestSequence(std::uint64_t previous_request_id,
                                    std::uint64_t request_id,
                                    ControlProtocolError* error) {
  if (request_id == 0 || request_id <= previous_request_id) {
    SetError(error, ControlProtocolError::kRequestOutOfSequence);
    return false;
  }
  SetError(error, ControlProtocolError::kNone);
  return true;
}

}  // namespace wubipinyin
