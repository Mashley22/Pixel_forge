module;

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <span>
#include <optional>

#include <PixelForge/core/macros.hpp>

export module PixelForge.core:buffer;

import :errors;
import :meta;
import :utils.fmt;

export namespace pf {

template <typename T_ptr>
concept PointerLike_c = std::is_pointer_v<T_ptr> ||
std::is_same_v<std::uintptr_t, T_ptr> ||
std::is_same_v<std::intptr_t, T_ptr> ||
std::is_same_v<std::ptrdiff_t, T_ptr>;

template <PointerLike_c T_to, PointerLike_c T_from>
[[nodiscard]] constexpr T_to
pointer_cast(T_from ptr) PF_NOEXCEPT {
  if (std::is_constant_evaluated()) {
    return std::bit_cast<T_to>(ptr);
  }
  return reinterpret_cast<T_to>(ptr);
}

template <typename T>
[[nodiscard]] constexpr bool 
isAligned(void* ptr) PF_NOEXCEPT {
  return (pointer_cast<std::uintptr_t>(ptr) % alignof(T)) == 0;
}

template <typename T>
[[nodiscard]] constexpr bool 
isAligned(std::byte* ptr) PF_NOEXCEPT {
  return isAligned<T>(pointer_cast<void*>(ptr));
}

template <typename T>
[[nodiscard]] constexpr bool 
isAligned(char* ptr) PF_NOEXCEPT {
  return isAligned<T>(pointer_cast<void*>(ptr));
}

template <typename T>
[[nodiscard]] constexpr bool 
isAligned(unsigned char* ptr) PF_NOEXCEPT {
  return isAligned<T>(pointer_cast<void*>(ptr));
}

/**
 *@brief A non-owning view of a buffer (raw memory) intended as storage space
 *       to size elements of type T. Intended to be constructed from BufferSpan 
 *       raw, as this class is an intermediary class simpy for type safety rather than
 *       passing in raw buffers.
 *
 */
template<typename T>
struct ObjectStorage {
public:
  using size_type = std::size_t;
  // no derefencing (atleast automatically)
  using pointer = void*;
  
  pointer data{nullptr};
  size_type size{0};
    
  [[nodiscard]] constexpr
  operator std::span<T>() const PF_NOEXCEPT {
    return {pointer_cast<T*>(data), size};
  }
};

struct Buffer {
  using pointer = std::byte*;
  using size_type = std::size_t;
  pointer data{nullptr};
  size_type size{0};

  struct Error : public Exception {
  private:
    template <std::size_t T>
    [[nodiscard]] constexpr std::string_view 
    strViewFromFmt(const FmtResult<T>& fmtResult) PF_NOEXCEPT {
      return {fmtResult.str, fmtResult.size};
    }
  public:
    template <std::size_t T> constexpr 
    Error(const FmtResult<T>& str) : Exception(strViewFromFmt(str)) PF_NOEXCEPT {}
  };

  struct AlignmentError : Error {
  public:
    static constexpr std::string_view what_fmt = 
      "Object store creation alignment error, required: {}, got ptr: {}";

  private:
    
    static constexpr size_type charCountFor64BitInt = 32;
    static constexpr size_type fmtBufSize = what_fmt.size() + 2 * charCountFor64BitInt;

  public:
  
    size_type requiredAlignment{0};
    std::uintptr_t ptrVal{0};
    
    constexpr 
    AlignmentError(size_type required_alignment, std::uintptr_t ptr_val) 
      : Error(fmt<fmtBufSize>(what_fmt, required_alignment, ptr_val)), 
      requiredAlignment(required_alignment), 
      ptrVal(ptr_val)
    PF_NOEXCEPT {
    }
  };

  struct SizeError : Error {
  public:
    static constexpr std::string_view what_fmt = 
      "Object store creation size error {} bytes supplied for {} objects of size {}";

  private:
    static constexpr size_type charCountFor64BitInt = 32;
    static constexpr size_type fmtBufSize = what_fmt.size() + 3 * charCountFor64BitInt;

  public:
    size_type bufferSize{0};
    size_type numObjects{0};
    size_type objectSize{0};
    
    constexpr 
    SizeError(size_type buf_size, size_type num_objs, size_type obj_size) 
      : Error(fmt<fmtBufSize>(what_fmt, buf_size, num_objs, obj_size)),
      bufferSize(buf_size), numObjects(num_objs), objectSize(obj_size)
    PF_NOEXCEPT {}
  };
  

  template <PointerLike_c T_ptr>
  [[nodiscard]] constexpr pointer
  toPtr_t(T_ptr ptr) PF_NOEXCEPT {
    return pointer_cast<pointer>(ptr); 
  }

  template <typename T, typename T_AlignmentErrPolicy = ErrPolicy_throws<ObjectStorage<T>, AlignmentError>, typename T_SizeErrPolicy = ErrPolicy_throws<ObjectStorage<T>, SizeError>>
  requires ErrPolicy_c<T_AlignmentErrPolicy, ObjectStorage<T>> && 
  ErrPolicy_c<T_SizeErrPolicy, ObjectStorage<T>> &&
  std::is_same_v<typename T_AlignmentErrPolicy::return_type, typename T_SizeErrPolicy::return_type> &&
  requires(size_type required_alignment, std::uintptr_t ptr_val, size_type bufSize, size_type objNum, size_type objSize) {
    { T_AlignmentErrPolicy::fail(required_alignment, ptr_val) } -> std::same_as<typename T_AlignmentErrPolicy::return_type>;
    { T_SizeErrPolicy::fail(bufSize, objNum , objSize) } -> std::same_as<typename T_SizeErrPolicy::return_type>;
  }
  [[nodiscard]] constexpr T_AlignmentErrPolicy::return_type
  asObjects(size_type num_objs) PF_NOEXCEPT_COND(T_SizeErrPolicy::is_noexcept && T_AlignmentErrPolicy::is_noexcept) {

    PF_CHECK_ERR_POLICY(T_AlignmentErrPolicy, !isAligned<T>(data), alignof(T), pointer_cast<std::uintptr_t>(data));
    
    PF_CHECK_ERR_POLICY(T_SizeErrPolicy, size_type requiredSize = num_objs * sizeof(T); requiredSize > size, size, num_objs, sizeof(T));

    return T_AlignmentErrPolicy::success({ .data = pointer_cast<void*>(data), .size = num_objs });
  }
  
  template <typename T>
  [[nodiscard]] constexpr ObjectStorage<T>
  asObjects_unchecked(size_type num_objs) PF_NOEXCEPT {
    static constexpr std::string_view alignment_issue = "Alignemnt issue";
    static constexpr std::string_view size_issue = "Incorrect size";
    return asObjects<T, ErrPolicy_nothing<ObjectStorage<T>, alignment_issue>, ErrPolicy_nothing<ObjectStorage<T>, size_issue>>(num_objs);
  }

  template <typename T>
  [[nodiscard]] constexpr std::optional<ObjectStorage<T>>
  try_asObjects(size_type num_objs) PF_NOEXCEPT {
    return asObjects<T, ErrPolicy_optional<ObjectStorage<T>>, ErrPolicy_optional<ObjectStorage<T>>>(num_objs);
  }
};

}
