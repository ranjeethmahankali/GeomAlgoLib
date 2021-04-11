#include <galfunc/GeomFunctions.h>
#include <galfunc/MeshFunctions.h>
#include <galview/AllViews.h>
#include <galview/Context.h>
#include <galview/GuiFunctions.h>

namespace gal {
namespace viewfunc {

static view::Panel*          sInputPanel  = nullptr;
static view::Panel*          sOutputPanel = nullptr;
static std::vector<uint64_t> mShowFuncRegs;

void initPanels(view::Panel& inputs, view::Panel& outputs)
{
  sInputPanel  = &inputs;
  sOutputPanel = &outputs;
};

view::Panel& inputPanel()
{
  return *sInputPanel;
};

view::Panel& outputPanel()
{
  return *sOutputPanel;
};

void evalOutputs()
{
  for (uint64_t id : mShowFuncRegs) {
    auto success = gal::func::store::get<bool>(id);
  }
};

template<typename T, typename... TRest>
struct DrawableManager
{
  static size_t draw(uint64_t typeId, const std::shared_ptr<void>& ptr, size_t oldDrawId)
  {
    static_assert(gal::func::types::TypeInfo<T>::value, "Unknown type");
    if (typeId == gal::func::types::TypeInfo<T>::id) {
      auto castsp = std::static_pointer_cast<T>(ptr);
      return view::Context::get().replaceDrawable<T>(oldDrawId, *castsp);
    }
    else if constexpr (sizeof...(TRest) > 0) {
      return DrawableManager<TRest...>::draw(typeId, ptr, oldDrawId);
    }
    else if constexpr (sizeof...(TRest) == 0) {
      std::cerr << "Datatype " << gal::func::types::TypeInfo<T>::name
                << " is not a drawable object\n";
      throw std::bad_cast();
    }
  };
};
using dmanager = DrawableManager<gal::Box3,
                                 gal::PointCloud,
                                 gal::Sphere,
                                 gal::Circle2d,
                                 gal::Mesh,
                                 gal::Plane>;

ShowFunc::ShowFunc(uint64_t regId)
    : mObjRegId(regId)
    , mSuccess(std::make_shared<bool>(false))
{
  gal::func::store::useRegister(this, mObjRegId);
};

void ShowFunc::initOutputRegisters()
{
  mRegisterId = gal::func::store::allocate(
    this, gal::func::types::TypeInfo<bool>::id, gal::func::types::TypeInfo<bool>::name);
};

void ShowFunc::run()
{
  try {
    auto  obj = gal::func::store::get<void>(mObjRegId);
    auto& reg = gal::func::store::getRegister(mObjRegId);
    mDrawId   = dmanager::draw(reg.typeId, reg.ptr, mDrawId);
    *mSuccess = true;
  }
  catch (std::bad_alloc ex) {
    *mSuccess = false;
  }
  gal::func::store::set<bool>(mRegisterId, mSuccess);
};

size_t ShowFunc::numOutputs() const
{
  return 1;
};

uint64_t ShowFunc::outputRegister(size_t index) const
{
  if (index == 0) {
    return mRegisterId;
  }
  throw std::out_of_range("Index out of range");
};

// Manual definition for show function because it has special needs.
gal::func::types::OutputTuple<1> show(const gal::func::store::Register& reg)
{
  using namespace gal::func;
  auto fn = store::makeFunction<ShowFunc>(reg.id);
  mShowFuncRegs.push_back(fn->outputRegister(0));
  return types::makeOutputTuple<1>(*fn);
};
boost::python::tuple py_show(gal::func::store::Register reg)
{
  return gal::func::pythonRegisterTuple(show(reg));
};

// Simple functions that are not part of the functional system.
void py_set2dMode(bool flag)
{
  gal::view::Context::get().set2dMode(flag);
};

}  // namespace viewfunc
}  // namespace gal

BOOST_PYTHON_MODULE(pygalview)
{
  using namespace boost::python;
  using namespace gal::viewfunc;
  //   class_<gal::func::store::Register>("Register").def(self_ns::str(self_ns::self));
  // Bindings related to gui
  // Views for drawables
  // Labels for strings
  // Sliders for float input
  // Text fields for string inputs
  def("sliderf32", gal::viewfunc::pySliderFn<float>);
  def("slideri32", gal::viewfunc::pySliderFn<int32_t>);
  GAL_DEF_PY_FN(show);
  GAL_DEF_PY_FN(set2dMode);
};