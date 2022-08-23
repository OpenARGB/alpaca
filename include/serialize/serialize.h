#pragma once
#include <serialize/detail/aggregate_arity.h>
#include <serialize/detail/from_bytes.h>
#include <serialize/detail/print_bytes.h>
#include <serialize/detail/struct_nth_field.h>
#include <serialize/detail/to_bytes.h>
#include <serialize/detail/type_traits.h>
#include <serialize/detail/variable_length_encoding.h>

template <typename T, std::size_t index>
void serialize(const T &s, std::vector<uint8_t> &bytes);

template <typename T>
void to_bytes_from_pair_type(const T &input, std::vector<uint8_t> &bytes);

template <typename T>
void to_bytes_from_tuple_type(const T &input, std::vector<uint8_t> &bytes);

template <typename T>
void to_bytes_from_list_type(const T &input, std::vector<uint8_t> &bytes);

template <typename T>
void to_bytes_router(const T &input, std::vector<uint8_t> &bytes) {
  // unsigned or signed integer types
  // char, bool
  // float, double
  if constexpr (std::is_arithmetic_v<T>) {
    // use variable-length encoding if possible
    detail::to_bytes(input, bytes);
  }
  // pair
  else if constexpr (detail::is_pair<T>::value) {
    to_bytes_from_pair_type<T>(input, bytes);
  }
  // std::string
  else if constexpr (detail::is_string::detect<T>) {
    detail::to_bytes(input, bytes);
  }
  // tuple
  else if constexpr (detail::is_tuple<T>::value) {
    to_bytes_from_tuple_type<T>(input, bytes);
  }
  // vector
  else if constexpr (detail::is_vector<T>::value) {
    to_bytes_from_list_type<T>(input, bytes);
  }
  // nested struct
  else if constexpr (std::is_class_v<T>) {
    serialize<T, 0>(input, bytes);
  } else {
    /// TODO: throw error unsupported type
    detail::append(input, bytes);
  }
}

// Specialization for pair

template <typename T>
void save_pair_value(const T &pair, std::vector<uint8_t> &bytes) {
  to_bytes_router(pair.first, bytes);
  to_bytes_router(pair.second, bytes);
}

template <typename T>
void to_bytes_from_pair_type(const T &input, std::vector<uint8_t> &bytes) {
  // value of each element
  save_pair_value<T>(input, bytes);
}

// Specialization for tuple

template <typename T, std::size_t index>
void save_tuple_value(const T &tuple, std::vector<uint8_t> &bytes) {
  constexpr auto max_index = std::tuple_size<T>::value;
  if constexpr (index < max_index) {
    to_bytes_router(std::get<index>(tuple), bytes);
    save_tuple_value<T, index + 1>(tuple, bytes);
  }
}

template <typename T>
void to_bytes_from_tuple_type(const T &input, std::vector<uint8_t> &bytes) {
  // value of each element
  save_tuple_value<T, 0>(input, bytes);
}

// Specialization for vector

template <typename T>
void to_bytes_from_list_type(const T &input, std::vector<uint8_t> &bytes) {
  // save vector size
  detail::to_bytes(input.size(), bytes);

  // value of each element in list
  for (const auto &v : input) {
    // check if the value_type is a nested list type
    using decayed_value_type = typename std::decay<decltype(v)>::type;
    to_bytes_router<decayed_value_type>(v, bytes);
  }
}

template <typename T, std::size_t index>
void serialize(const T &s, std::vector<uint8_t> &bytes) {
  constexpr static auto max_index =
      detail::aggregate_arity<std::remove_cv_t<T>>::size();
  if constexpr (index < max_index) {
    decltype(auto) field = detail::get<index>(s);
    using decayed_field_type = typename std::decay<decltype(field)>::type;

    // serialize field
    to_bytes_router<decayed_field_type>(field, bytes);

    // go to next field
    serialize<T, index + 1>(s, bytes);
  }
}

template <typename T> std::vector<uint8_t> serialize(T &s) {
  std::vector<uint8_t> bytes{};
  serialize<T, 0>(s, bytes);
  return bytes;
}

// Forward declares
template <typename T, std::size_t index>
void deserialize(T &s, const std::vector<uint8_t> &bytes,
                 std::size_t byte_index);

template <typename T>
bool from_bytes_to_pair(T &pair, const std::vector<uint8_t> &bytes,
                        std::size_t &current_index);

template <typename T>
bool from_bytes_to_tuple(T &tuple, const std::vector<uint8_t> &bytes,
                         std::size_t &current_index);

template <typename T>
bool from_bytes_to_vector(std::vector<T> &value,
                          const std::vector<uint8_t> &bytes,
                          std::size_t &current_index);

template <typename T>
void from_bytes_router(T &output, const std::vector<uint8_t> &bytes,
                       std::size_t &byte_index) {

  // unsigned or signed integer types
  // char, bool
  // float, double
  if constexpr (std::is_arithmetic_v<T>) {
    detail::from_bytes(output, bytes, byte_index);
  }
  // pair
  else if constexpr (detail::is_pair<T>::value) {
    from_bytes_to_pair(output, bytes, byte_index);
  }
  // std::string
  else if constexpr (detail::is_string::detect<T>) {
    detail::from_bytes(output, bytes, byte_index);
  }
  // tuple
  else if constexpr (detail::is_tuple<T>::value) {
    from_bytes_to_tuple(output, bytes, byte_index);
  }
  // vector
  else if constexpr (detail::is_vector<T>::value) {
    from_bytes_to_vector(output, bytes, byte_index);
  }
  // nested struct
  else if constexpr (std::is_class_v<T>) {
    deserialize<T, 0>(output, bytes, byte_index);
  } else {
    /// TODO: throw error unsupported type
    detail::from_bytes(output, bytes, byte_index);
  }
}

// Specialization for pair

template <typename T>
void load_pair_value(T &pair, const std::vector<uint8_t> &bytes,
                     std::size_t &current_index) {
  from_bytes_router(pair.first, bytes, current_index);
  from_bytes_router(pair.second, bytes, current_index);
}

template <typename T>
bool from_bytes_to_pair(T &pair, const std::vector<uint8_t> &bytes,
                        std::size_t &current_index) {
  load_pair_value<T>(pair, bytes, current_index);
  return true;
}

// Specialization for tuple

template <typename T, std::size_t index>
void load_tuple_value(T &tuple, const std::vector<uint8_t> &bytes,
                      std::size_t &current_index) {
  constexpr auto max_index = std::tuple_size<T>::value;
  if constexpr (index < max_index) {
    from_bytes_router(std::get<index>(tuple), bytes, current_index);
    load_tuple_value<T, index + 1>(tuple, bytes, current_index);
  }
}

template <typename T>
bool from_bytes_to_tuple(T &tuple, const std::vector<uint8_t> &bytes,
                         std::size_t &current_index) {
  load_tuple_value<T, 0>(tuple, bytes, current_index);
  return true;
}

// Specialization for vector

template <typename T>
bool from_bytes_to_vector(std::vector<T> &value,
                          const std::vector<uint8_t> &bytes,
                          std::size_t &current_index) {

  // current byte is the size of the vector
  std::size_t size = detail::decode_varint<std::size_t>(bytes, current_index);

  // read `size` bytes and save to value
  for (std::size_t i = 0; i < size; ++i) {
    T v{};
    from_bytes_router(v, bytes, current_index);
    value.push_back(v);
  }

  return true;
}

template <typename T, std::size_t index>
void deserialize(T &s, const std::vector<uint8_t> &bytes,
                 std::size_t byte_index) {
  constexpr static auto max_index =
      detail::aggregate_arity<std::remove_cv_t<T>>::size();
  if constexpr (index < max_index) {
    decltype(auto) field = detail::get<index>(s);
    using decayed_field_type = typename std::decay<decltype(field)>::type;

    // load current field
    from_bytes_router<decayed_field_type>(field, bytes, byte_index);

    // go to next field
    deserialize<T, index + 1>(s, bytes, byte_index);
  }
}

template <typename T> T deserialize(const std::vector<uint8_t> &bytes) {
  T object{};
  deserialize<T, 0>(object, bytes, 0);
  return object;
}