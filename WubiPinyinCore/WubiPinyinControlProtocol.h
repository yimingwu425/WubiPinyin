#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wubipinyin {

// Encoded little-endian, this appears on the wire as the ASCII bytes "WBPC".
inline constexpr std::uint32_t kControlProtocolMagic = 0x43504257;
inline constexpr std::uint16_t kControlProtocolVersion = 1;
inline constexpr std::size_t kControlFrameHeaderBytes = 36;
inline constexpr std::size_t kControlFramePayloadLengthOffset = 32;
inline constexpr std::size_t kMaxControlPayloadBytes = 64 * 1024;

enum class ControlMessageType : std::uint16_t {
  kGetSettings = 1,
  kUpdateSetting,
  kListUserEntries,
  kUpsertUserEntry,
  kDeleteUserEntry,
  kResetLearning,
  kSetRoute,
  kCommitRaw,
  kReloadDictionaries,
  kResponse = 0x8000,
  kError,
};

enum class ControlProtocolError : std::uint8_t {
  kNone,
  kInvalidArgument,
  kInvalidMagic,
  kUnsupportedVersion,
  kInvalidMessageType,
  kInvalidRequestId,
  kPayloadTooLarge,
  kTruncatedHeader,
  kTruncatedPayload,
  kTrailingBytes,
  kRequestOutOfSequence,
};

struct ControlFrame {
  ControlMessageType type = ControlMessageType::kGetSettings;
  std::uint64_t request_id = 0;
  std::uint64_t session_id = 0;
  std::uint64_t generation = 0;
  std::vector<std::uint8_t> payload;
};

// Encodes a complete control frame. The output is unchanged when validation
// fails, so callers can safely reuse a buffer after a rejected request.
bool EncodeControlFrame(const ControlFrame& frame,
                        std::vector<std::uint8_t>* encoded,
                        ControlProtocolError* error);

// Decodes exactly one complete message-mode control frame. It rejects trailing
// bytes so a pipe message cannot be interpreted as more than one request.
bool DecodeControlFrame(const std::uint8_t* encoded,
                        std::size_t encoded_size,
                        ControlFrame* frame,
                        ControlProtocolError* error);

// Request ids are monotonic per connection. Responses reuse their request id
// and therefore are not validated with this request-side helper.
bool ValidateControlRequestSequence(std::uint64_t previous_request_id,
                                    std::uint64_t request_id,
                                    ControlProtocolError* error);

}  // namespace wubipinyin
