module;

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:buffer;

import :errors;
import :meta;
import :pointers;
import :utils.fmt;

export namespace pf {

/**
 *@brief A non-owning view of a buffer (raw memory) intended as storage space
 *       to size elements of type T. Intended to be constructed from BufferSpan
 *       raw, as this class is an intermediary class simpy for type safety rather than
 *       passing in raw buffers.
 *
 */
template <typename T>
struct ObjectStorage {
public:
  using SizeType = std::size_t;
  // no derefencing (atleast automatically)
  using Pointer = void*;

  Pointer data{nullptr};
  SizeType size{0};

  [[nodiscard]] constexpr T&
  operator[](SizeType idx) PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(idx < size);
    return *std::launder(pointer_cast<T*>(data) + idx);
  }

  [[nodiscard]] PF_PURE_FUNC constexpr const T&
  operator[](SizeType idx) const PF_NOEXCEPT {
    PF_REQUIRE_ASSUME(idx < size);
    return *std::launder(pointer_cast<T*>(data) + idx);
  }
};

struct Buffer {
  using Pointer = std::byte*;
  using SizeType = std::size_t;
  Pointer data{nullptr};
  SizeType size{0};

  static constexpr Buffer
  null() {
    return Buffer{.data = nullptr, .size = 0};
  }

  [[nodiscard]] constexpr bool
  is_null() const PF_NOEXCEPT {
    return data == nullptr;
  }

  static constexpr Buffer
  from(Pointer ptr, SizeType sze) PF_NOEXCEPT {
    return Buffer{.data = ptr, .size = sze};
  }

  static constexpr Buffer
  from(std::span<std::byte> buf) PF_NOEXCEPT {
    return Buffer{.data = buf.data(), .size = buf.size()};
  }

  struct Error : public Exception {
  public:
    template <std::size_t BufLen>
    constexpr Error(const FmtResult<BufLen>& str)
      : Exception(str.to_str_view()) PF_NOEXCEPT {}
  };

  struct AlignmentError : Error {
  public:
    static constexpr std::string_view what_fmt =
        "Object store creation alignment error, required: {}, got ptr: {}";

  private:
    static constexpr SizeType char_count_for64_bit_int = 32;
    static constexpr SizeType fmt_buf_size =
        what_fmt.size() + 2 * char_count_for64_bit_int;

  public:
    SizeType required_alignment{0};
    std::uintptr_t ptr_val{0};

    constexpr AlignmentError(SizeType alignment, std::uintptr_t ptr)
      : Error(fmt<fmt_buf_size>(what_fmt, alignment, ptr)), required_alignment(alignment),
        ptr_val(ptr) PF_NOEXCEPT {}
  };

  struct SizeError : Error {
  public:
    static constexpr std::string_view what_fmt =
        "Object store creation size error {} bytes supplied for {} objects of size {}";

  private:
    static constexpr SizeType char_count_for64_bit_int = 32;
    static constexpr SizeType fmt_buf_size =
        what_fmt.size() + 3 * char_count_for64_bit_int;

  public:
    SizeType buffer_size{0};
    SizeType num_objects{0};
    SizeType object_size{0};

    constexpr SizeError(SizeType buf_size, SizeType num_objs, SizeType obj_size)
      : Error(fmt<fmt_buf_size>(what_fmt, buf_size, num_objs, obj_size)),
        buffer_size(buf_size), num_objects(num_objs), object_size(obj_size) PF_NOEXCEPT {}
  };

  template <PointerLike_c PtrT>
  [[nodiscard]] constexpr Pointer
  to_ptr_t(PtrT ptr) PF_NOEXCEPT {
    return pointer_cast<Pointer>(ptr);
  }

  template <
      typename T,
      typename AlignmentErrPolicy = ErrPolicyThrows<ObjectStorage<T>, AlignmentError>,
      typename SizeErrPolicy = ErrPolicyThrows<ObjectStorage<T>, SizeError>>
    requires ErrPolicy_c<AlignmentErrPolicy, ObjectStorage<T>> &&
             ErrPolicy_c<SizeErrPolicy, ObjectStorage<T>> &&
             std::is_same_v<typename AlignmentErrPolicy::ReturnType,
                            typename SizeErrPolicy::ReturnType> &&
             requires(SizeType required_alignment,
                      std::uintptr_t ptr_val,
                      SizeType buf_size,
                      SizeType obj_num,
                      SizeType obj_size) {
               {
                 AlignmentErrPolicy::fail(required_alignment, ptr_val)
               } -> std::same_as<typename AlignmentErrPolicy::ReturnType>;
               {
                 SizeErrPolicy::fail(buf_size, obj_num, obj_size)
               } -> std::same_as<typename SizeErrPolicy::ReturnType>;
             }
  [[nodiscard]] constexpr AlignmentErrPolicy::ReturnType
  as_objects(SizeType num_objs)
      PF_NOEXCEPT_COND(T_SizeErrPolicy::is_noexcept&& T_AlignmentErrPolicy::is_noexcept) {

    PF_CHECK_ERR_POLICY(AlignmentErrPolicy,
                        !is_aligned<T>(data),
                        alignof(T),
                        pointer_cast<std::uintptr_t>(data));

    PF_CHECK_ERR_POLICY(SizeErrPolicy, SizeType required_size = num_objs * sizeof(T);
                        required_size > size, size, num_objs, sizeof(T));

    return AlignmentErrPolicy::success(
        {.data = pointer_cast<void*>(data), .size = num_objs});
  }

  template <typename T>
  [[nodiscard]] constexpr ObjectStorage<T>
  as_objects_unchecked(SizeType num_objs) PF_NOEXCEPT {
    static constexpr std::string_view alignment_issue = "Alignemnt issue";
    static constexpr std::string_view size_issue = "Incorrect size";
    return as_objects<T,
                      ErrPolicyNothing<ObjectStorage<T>, alignment_issue>,
                      ErrPolicyNothing<ObjectStorage<T>, size_issue>>(num_objs);
  }

  template <typename T>
  [[nodiscard]] constexpr std::optional<ObjectStorage<T>>
  try_as_objects(SizeType num_objs) PF_NOEXCEPT {
    return as_objects<T,
                      ErrPolicyOptional<ObjectStorage<T>>,
                      ErrPolicyOptional<ObjectStorage<T>>>(num_objs);
  }
};

}
