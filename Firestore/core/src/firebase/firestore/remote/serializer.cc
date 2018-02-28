/*
 * Copyright 2018 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Firestore/core/src/firebase/firestore/remote/serializer.h"

#include <pb_decode.h>
#include <pb_encode.h>

namespace firebase {
namespace firestore {
namespace remote {

namespace {

/**
 * Note that (despite the value parameter type) this works for bool, enum,
 * int32, int64, uint32 and uint64 proto field types.
 *
 * Note: This is not expected to be called direclty, but rather only via the
 * other Encode* methods (i.e. EncodeBool, EncodeLong, etc)
 *
 * @param value The value to encode, represented as a uint64_t.
 */
void EncodeVarint(pb_ostream_t* stream, uint64_t value) {
  bool status = pb_encode_varint(stream, value);
  if (!status) {
    // TODO(rsgowman): figure out error handling
    abort();
  }
}

/**
 * Note that (despite the return type) this works for bool, enum, int32, int64,
 * uint32 and uint64 proto field types.
 *
 * Note: This is not expected to be called direclty, but rather only via the
 * other Decode* methods (i.e. DecodeBool, DecodeLong, etc)
 *
 * @return The decoded varint as a uint64_t.
 */
uint64_t DecodeVarint(pb_istream_t* stream) {
  uint64_t varint_value;
  bool status = pb_decode_varint(stream, &varint_value);
  if (!status) {
    // TODO(rsgowman): figure out error handling
    abort();
  }
  return varint_value;
}

void EncodeNull(pb_ostream_t* stream) {
  return EncodeVarint(stream, google_protobuf_NullValue_NULL_VALUE);
}

void DecodeNull(pb_istream_t* stream) {
  uint64_t varint = DecodeVarint(stream);
  if (varint != google_protobuf_NullValue_NULL_VALUE) {
    // TODO(rsgowman): figure out error handling
    abort();
  }
}

void EncodeBool(pb_ostream_t* stream, bool bool_value) {
  return EncodeVarint(stream, bool_value);
}

bool DecodeBool(pb_istream_t* stream) {
  uint64_t varint = DecodeVarint(stream);
  switch (varint) {
    case 0:
      return false;
    case 1:
      return true;
    default:
      // TODO(rsgowman): figure out error handling
      abort();
  }
}

void EncodeInteger(pb_ostream_t* stream, int64_t integer_value) {
  return EncodeVarint(stream, integer_value);
}

int64_t DecodeInteger(pb_istream_t* stream) {
  return DecodeVarint(stream);
}

}  // namespace

using firebase::firestore::model::FieldValue;

void Serializer::EncodeFieldValue(const FieldValue& field_value,
                                  std::vector<uint8_t>* out_bytes) {
  // TODO(rsgowman): how large should the output buffer be? Do some
  // investigation to see if we can get nanopb to tell us how much space it's
  // going to need.
  uint8_t buf[1024];
  pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));

  // TODO(rsgowman): some refactoring is in order... but will wait until after a
  // non-varint, non-fixed-size (i.e. string) type is present before doing so.
  bool status = false;
  switch (field_value.type()) {
    case FieldValue::Type::Null:
      status = pb_encode_tag(&stream, PB_WT_VARINT,
                             google_firestore_v1beta1_Value_null_value_tag);
      if (!status) {
        // TODO(rsgowman): figure out error handling
        abort();
      }
      EncodeNull(&stream);
      break;

    case FieldValue::Type::Boolean:
      status = pb_encode_tag(&stream, PB_WT_VARINT,
                             google_firestore_v1beta1_Value_boolean_value_tag);
      if (!status) {
        // TODO(rsgowman): figure out error handling
        abort();
      }
      EncodeBool(&stream, field_value.boolean_value());
      break;

    case FieldValue::Type::Integer:
      status = pb_encode_tag(&stream, PB_WT_VARINT,
                             google_firestore_v1beta1_Value_integer_value_tag);
      if (!status) {
        // TODO(rsgowman): figure out error handling
        abort();
      }
      EncodeInteger(&stream, field_value.integer_value());
      break;

    default:
      // TODO(rsgowman): implement the other types
      abort();
  }

  out_bytes->insert(out_bytes->end(), buf, buf + stream.bytes_written);
}

FieldValue Serializer::DecodeFieldValue(const uint8_t* bytes, size_t length) {
  pb_istream_t stream = pb_istream_from_buffer(bytes, length);
  pb_wire_type_t wire_type;
  uint32_t tag;
  bool eof;
  bool status = pb_decode_tag(&stream, &wire_type, &tag, &eof);
  if (!status || wire_type != PB_WT_VARINT) {
    // TODO(rsgowman): figure out error handling
    abort();
  }

  switch (tag) {
    case google_firestore_v1beta1_Value_null_value_tag:
      DecodeNull(&stream);
      return FieldValue::NullValue();
    case google_firestore_v1beta1_Value_boolean_value_tag:
      return FieldValue::BooleanValue(DecodeBool(&stream));
    case google_firestore_v1beta1_Value_integer_value_tag:
      return FieldValue::IntegerValue(DecodeInteger(&stream));

    default:
      // TODO(rsgowman): figure out error handling
      abort();
  }
}

}  // namespace remote
}  // namespace firestore
}  // namespace firebase
