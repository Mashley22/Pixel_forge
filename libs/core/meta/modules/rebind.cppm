export module PixelForge.core:meta.rebind;

export namespace pf {

template <typename T>
struct Rebind;

template <template <typename> class T, typename T_arg>
struct Rebind<T<T_arg>> {
  template <typename T_newArg>
  using to = T<T_newArg>;
};

}
