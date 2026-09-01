export module PixelForge.core:meta.rebind;

export namespace pf {

template <typename T>
struct Rebind;

template <template <typename> class T, typename T_arg>
struct Rebind<T<T_arg>> {
  template <typename T_newArg>
  using to = T<T_newArg>;
};

template <template <typename, typename> class T, typename T_arg1, typename T_arg2>
struct Rebind<T<T_arg1, T_arg2>> {
  template <typename T_newArg1, typename T_newArg2>
  using to = T<T_newArg1, T_newArg2>;
};

}
