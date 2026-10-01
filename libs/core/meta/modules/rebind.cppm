export module PixelForge.core:meta.rebind;

export namespace pf {

template <typename Bound>
struct Rebind;

template <template <typename> class Template, typename Arg>
struct Rebind<Template<Arg>> {
  template <typename NewArg>
  using To = Template<NewArg>;
};

template <template <typename, typename> class Template, typename Arg1, typename Arg2>
struct Rebind<Template<Arg1, Arg2>> {
  template <typename NewArg1, typename NewArg2>
  using To = Template<NewArg1, NewArg2>;
};

}