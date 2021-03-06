#include <galfunc/GeomFunctions.h>
#include <galfunc/MeshFunctions.h>
#include <galview/AllViews.h>
#include <galview/Context.h>
#include <galview/GuiFunctions.h>
#include <sstream>

namespace gal {
namespace viewfunc {

static view::Panel*          sInputPanel  = nullptr;
static view::Panel*          sOutputPanel = nullptr;
static std::vector<uint64_t> sShowFuncRegs;
static std::vector<uint64_t> sPrintFuncRegs;

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
  for (uint64_t id : sShowFuncRegs) {
    auto success = gal::func::store::get<bool>(id);
  }
  for (uint64_t id : sPrintFuncRegs) {
    auto success = gal::func::store::get<bool>(id);
  }
};

template<typename T, typename... TRest>
struct DrawableManager
{
  static size_t draw(uint64_t                     typeId,
                     const std::shared_ptr<void>& ptr,
                     size_t                       oldDrawId,
                     const bool*                  visibility)
  {
    static_assert(gal::TypeInfo<T>::value, "Unknown type");
    if (typeId == gal::TypeInfo<T>::id) {
      auto castsp = std::static_pointer_cast<T>(ptr);
      return view::Context::get().replaceDrawable<T>(oldDrawId, *castsp, visibility);
    }
    else if constexpr (sizeof...(TRest) > 0) {
      return DrawableManager<TRest...>::draw(typeId, ptr, oldDrawId, visibility);
    }
    else if constexpr (sizeof...(TRest) == 0) {
      std::cerr << "Datatype " << gal::TypeInfo<T>::name()
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

ShowFunc::ShowFunc(const std::string& label, uint64_t regId)
    : mShowables(1, std::make_pair(regId, 0))
    , mSuccess(std::make_shared<bool>(false))
    , gal::view::CheckBox(label, true)
{
  useUpstreamRegisters();
};

ShowFunc::ShowFunc(const std::string& label, const std::vector<uint64_t>& regIds)
    : mShowables(regIds.size())
    , mSuccess(std::make_shared<bool>(false))
    , gal::view::CheckBox(label, true)
{
  std::transform(regIds.begin(), regIds.end(), mShowables.begin(), [](uint64_t reg) {
    return std::make_pair(reg, size_t(0));
  });

  useUpstreamRegisters();
}

void ShowFunc::useUpstreamRegisters()
{
  for (size_t i = 0; i < mShowables.size(); i++) {
    // Telling galfunc that this functor depends on this input register.
    gal::func::store::useRegister(this, mShowables[i].first);
  }
}

void ShowFunc::initOutputRegisters()
{
  mRegisterId = gal::func::store::allocate(
    this, gal::TypeInfo<bool>::id, gal::TypeInfo<bool>::name());
};

void ShowFunc::run()
{
  try {
    for (auto& showable : mShowables) {
      // Calling get triggers the upstream computations if needed.
      gal::func::store::get<void>(showable.first);
      auto& reg = gal::func::store::getRegister(showable.first);
      showable.second =
        dmanager::draw(reg.typeId, reg.ptr, showable.second, checkedPtr());
    }
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

TagsFunc::TagsFunc(const std::string& label, uint64_t locsRegId, uint64_t wordsRegId)
    : mLocsRegId(locsRegId)
    , mWordsRegId(wordsRegId)
    , mSuccess(std::make_shared<bool>(false))
    , gal::view::CheckBox(label, true)
{
  gal::func::store::useRegister(this, mLocsRegId);
  gal::func::store::useRegister(this, mWordsRegId);
}

void TagsFunc::initOutputRegisters()
{
  mRegisterId = gal::func::store::allocate(
    this, gal::TypeInfo<bool>::id, gal::TypeInfo<bool>::name());
}

void TagsFunc::run()
{
  try {
    // Calling get triggers the upstream computations if needed.
    gal::func::store::get<std::vector<glm::vec3>>(mLocsRegId);
    gal::func::store::get<std::vector<std::string>>(mWordsRegId);
    auto& locsReg  = gal::func::store::getRegister(mLocsRegId);
    auto& wordsReg = gal::func::store::getRegister(mWordsRegId);
    auto  locs     = std::static_pointer_cast<std::vector<glm::vec3>>(locsReg.ptr);
    auto  words    = std::static_pointer_cast<std::vector<std::string>>(wordsReg.ptr);

    size_t      ntags = std::min(locs->size(), words->size());
    Annotations tagvals;
    tagvals.reserve(ntags);
    for (size_t i = 0; i < ntags; i++) {
      tagvals.emplace_back(locs->at(i), words->at(i));
    }

    mDrawId   = gal::view::Context::get().replaceDrawable(mDrawId, tagvals, checkedPtr());
    *mSuccess = true;
  }
  catch (std::bad_alloc ex) {
    *mSuccess = false;
  }
  gal::func::store::set<bool>(mRegisterId, mSuccess);
}

size_t TagsFunc::numOutputs() const
{
  return 1;
}

uint64_t TagsFunc::outputRegister(size_t index) const
{
  if (index == 0) {
    return mRegisterId;
  }
  throw std::out_of_range("Index out of range");
}

template<typename T, typename... TRest>
struct PrintManager
{
  static std::string print(uint64_t typeId, const std::shared_ptr<void>& ptr)
  {
    static_assert(gal::TypeInfo<T>::value, "Unknown type");
    if (typeId == gal::TypeInfo<T>::id) {
      auto              castsp = std::static_pointer_cast<T>(ptr);
      std::stringstream ss;
      ss << *castsp;
      return ss.str();
    }
    else if (typeId == gal::TypeInfo<std::vector<T>>::id) {
      auto              castsp = std::static_pointer_cast<std::vector<T>>(ptr);
      std::stringstream ss;
      ss << *castsp;
      return ss.str();
    }
    else if constexpr (sizeof...(TRest) > 0) {
      return PrintManager<TRest...>::print(typeId, ptr);
    }
    else if constexpr (sizeof...(TRest) == 0) {
      std::cerr << "Datatype " << gal::TypeInfo<T>::name
                << " is not a printable object\n";
      throw std::bad_cast();
    }
  };
};

using printmanager = PrintManager<float, int32_t, glm::vec3, std::string>;

struct PrintFunc : public gal::func::Function, public gal::view::Text
{
private:
  std::string           mLabel;
  uint64_t              mObjRegId;
  std::shared_ptr<bool> mSuccess;
  uint64_t              mRegisterId;

public:
  PrintFunc(const std::string& label, uint64_t regId)
      : gal::view::Text("")
      , mLabel(label)
      , mObjRegId(regId)
      , mSuccess(std::make_shared<bool>(false))
  {
    gal::func::store::useRegister(this, mObjRegId);
  };

  void run() override
  {
    try {
      auto  obj    = gal::func::store::get<void>(mObjRegId);
      auto& reg    = gal::func::store::getRegister(mObjRegId);
      this->mValue = mLabel + ": " + printmanager::print(reg.typeId, reg.ptr);
      *mSuccess    = true;
    }
    catch (std::bad_alloc ex) {
      *mSuccess = false;
    }
    gal::func::store::set<bool>(mRegisterId, mSuccess);
  };
  void initOutputRegisters() override
  {
    mRegisterId = gal::func::store::allocate(
      this, gal::TypeInfo<bool>::id, gal::TypeInfo<bool>::name());
  };
  size_t   numOutputs() const override { return 1; };
  uint64_t outputRegister(size_t index) const override
  {
    if (index == 0) {
      return mRegisterId;
    }
    throw std::out_of_range("Index out of range");
  };
};

struct TextFieldFunc : public gal::func::TVariable<std::string>,
                       public gal::view::TextInput
{
public:
  TextFieldFunc(const std::string& label)
      : gal::func::TVariable<std::string>("")
      , gal::view::TextInput(label, "") {};

private:
  using gal::view::TextInput::addHandler;

protected:
  void handleChanges() override
  {
    if (isEdited())
      this->set(this->mValue);

    clearEdited();
  }
};

// Manual definition for show function because it has special needs.
gal::func::types::OutputTuple<1> show(const std::string&                label,
                                      const gal::func::store::Register& reg)
{
  using namespace gal::func;
  auto fn = store::makeFunction<ShowFunc>(label, reg.id);
  sShowFuncRegs.push_back(fn->outputRegister(0));
  auto wfn = std::dynamic_pointer_cast<gal::view::Widget>(fn);
  outputPanel().addWidget(std::dynamic_pointer_cast<gal::view::Widget>(fn));
  return types::makeOutputTuple<1>(*fn);
};

boost::python::tuple py_show(const std::string& label, gal::func::store::Register reg)
{
  return gal::func::pythonRegisterTuple(show(label, reg));
};

// Manual definition for show function because it has special needs.
gal::func::types::OutputTuple<1> showAll(
  const std::string&                             label,
  const std::vector<gal::func::store::Register>& regs)
{
  using namespace gal::func;
  std::vector<uint64_t> regIds(regs.size());
  std::transform(
    regs.begin(), regs.end(), regIds.begin(), [](const gal::func::store::Register& reg) {
      return reg.id;
    });
  auto fn = store::makeFunction<ShowFunc>(label, regIds);
  sShowFuncRegs.push_back(fn->outputRegister(0));
  auto wfn = std::dynamic_pointer_cast<gal::view::Widget>(fn);
  outputPanel().addWidget(std::dynamic_pointer_cast<gal::view::Widget>(fn));
  return types::makeOutputTuple<1>(*fn);
};

boost::python::tuple py_showAll(const std::string&         label,
                                const boost::python::list& regsPy)
{
  std::vector<gal::func::store::Register> regs;
  gal::func::Converter<boost::python::list, decltype(regs)>::assign(regsPy, regs);
  return gal::func::pythonRegisterTuple(showAll(label, regs));
};

// Manual definition for the print function because it has special needs.
gal::func::types::OutputTuple<1> print(const std::string&                label,
                                       const gal::func::store::Register& reg)
{
  using namespace gal::func;
  auto fn = store::makeFunction<PrintFunc>(label, reg.id);
  sPrintFuncRegs.push_back(fn->outputRegister(0));
  auto wfn = std::dynamic_pointer_cast<gal::view::Widget>(fn);
  outputPanel().addWidget(std::dynamic_pointer_cast<gal::view::Widget>(fn));
  return types::makeOutputTuple<1>(*fn);
};
boost::python::tuple py_print(const std::string& label, gal::func::store::Register reg)
{
  return gal::func::pythonRegisterTuple(print(label, reg));
};

// Simple functions that are not part of the functional system.
void py_set2dMode(bool flag)
{
  gal::view::Context::get().set2dMode(flag);
};

void py_useOrthoCam()
{
  gal::view::Context::get().setOrthographic();
}

void py_usePerspectiveCam()
{
  gal::view::Context::get().setPerspective();
}

gal::func::types::OutputTuple<1> textField(const std::string& label)
{
  auto fn = gal::func::store::makeFunction<TextFieldFunc>(label);
  inputPanel().addWidget(std::dynamic_pointer_cast<gal::view::Widget>(fn));
  return gal::func::types::makeOutputTuple<1>(*fn);
};

boost::python::tuple py_textField(const std::string& label)
{
  return gal::func::pythonRegisterTuple(textField(label));
};

gal::func::types::OutputTuple<1> tags(const std::string&                label,
                                      const gal::func::store::Register& locsReg,
                                      gal::func::store::Register&       wordsReg)
{
  using namespace gal::func;
  auto fn = store::makeFunction<TagsFunc>(label, locsReg.id, wordsReg.id);
  sShowFuncRegs.push_back(fn->outputRegister(0));
  auto wfn = std::dynamic_pointer_cast<gal::view::Widget>(fn);
  outputPanel().addWidget(std::dynamic_pointer_cast<gal::view::Widget>(fn));
  return types::makeOutputTuple<1>(*fn);
}

boost::python::tuple py_tags(const std::string&         label,
                             gal::func::store::Register locs,
                             gal::func::store::Register words)
{
  return gal::func::pythonRegisterTuple(tags(label, locs, words));
}

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
  def("sliderf32", gal::viewfunc::py_slider<float>);
  def("slideri32", gal::viewfunc::py_slider<int32_t>);
  GAL_DEF_PY_FN(show);
  GAL_DEF_PY_FN(showAll);
  GAL_DEF_PY_FN(print);
  GAL_DEF_PY_FN(textField);
  GAL_DEF_PY_FN(tags);
  GAL_DEF_PY_FN(set2dMode);
  GAL_DEF_PY_FN(useOrthoCam);
  GAL_DEF_PY_FN(usePerspectiveCam);
};
