module;

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

#include <PixelForge/core/macros.hpp>
#include <PixelForge/adapters/macros.hpp>

export module PixelForge.adapters:spscLLQueue;

import PixelForge.core;

namespace pf {
  template <typename T>
  class SPSCLLQueue {

  public:
    struct Node {
      std::atomic<Node*> pNext{nullptr};
      T val;
    };

    struct Error : public Exception {};
    struct EmptyError : public Exception {
      static constexpr std::string_view what_arg = "SPSCLLQueue: Attempted pop while empty";
      EmptyError() : Exception(what_arg) {}
    };

    struct Traits {
      using value_type = T;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using reference = value_type&;
      using const_reference = const value_type&;
      using pointer = T*;
      using const_pointer = const T*;
      using storage_type = T;

      static constexpr bool is_nothrow_copy_construct_v =
          std::is_nothrow_copy_constructible_v<T>;
      static constexpr bool is_nothrow_move_construct_v =
          std::is_nothrow_move_constructible_v<T>;
      template <typename... V_args>
      static constexpr bool is_nothrow_construct_v =
          std::is_nothrow_constructible_v<T, V_args...>;
    };

    PF_ADAPTERS_INHERIT_TRAITS(Traits);

    SPSCLLQueue();

    bool empty() PF_NOEXCEPT {
      return m_back->pNext.load(std::memory_order_acquire) = nullptr;
    }
  
    template<class... V_Args>
    void emplace(const ObjectStorage<storage_type>& storage, V_Args&&... args) 
    PF_NOEXCEPT_COND(template Traits::is_nothrow_construct_v) {
      PF_REQUIRE_ASSUME(storage.size == 1);

      Node* pNewNode = std::construct_at(storage.data, std::forward<V_Args>(args)...);
      m_back->pNext.store(pNewNode, std::memory_order_release);
      m_back = pNewNode;
    }

    void push(const ObjectStorage<storage_type>& storage, T&& val) PF_NOEXCEPT_COND(Traits::is_nothrow_move_construct_v) {
      emplace(storage, std::forward<T>(val));
    }

    void push(const ObjectStorage<storage_type>& storage, const T& val) PF_NOEXCEPT_COND(Traits::is_nothrow_copy_construct_v) {
      emplace(storage, val);
    }

    template <typename T_ErrPolicy = ErrPolicy_throws<Node*, EmptyError>>
    requires ErrPolicy_c<T_ErrPolicy, Node*> && requires {
      { T_ErrPolicy::fail() } -> std::same_as<typename T_ErrPolicy::return_type>;
    }
    Node* pop() PF_NOEXCEPT {
      Node* pNextNext = m_back->pNext.load(std::memory_order_acquire);

      PF_CHECK_ERR_POLICY(T_ErrPolicy, pNextNext == nullptr);

      Node* pReturnNode = m_back;
      m_back = pNextNext;

      return T_ErrPolicy::success(pReturnNode);
    }

    std::optional<Node*> try_pop() PF_NOEXCEPT {
      return pop<ErrPolicy_optional<Node*>>();
    }

    Node* pop_unchecked() PF_NOEXCEPT {
      return pop<ErrPolicy_nothing<Node*, EmptyError::what_arg>>();
    }

  private:
    PF_CACHE_LINE_ALIGN_VAR
    NonNull<Node*>
    m_front;

    PF_CACHE_LINE_ALIGN_VAR
    NonNull<Node*>
    m_back;
  };
}
