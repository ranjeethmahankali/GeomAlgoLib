#pragma once

#include <galfunc/MapMacro.h>
#include <string.h>
#include <functional>
#include <iostream>
#include <memory>

namespace gal {
namespace func {

// Interface.
struct Function
{
  virtual void     run()                              = 0;
  virtual void     initOutputRegisters()              = 0;
  virtual size_t   numOutputs() const                 = 0;
  virtual uint64_t outputRegister(size_t index) const = 0;
};

namespace types {

// Can be used to check at compile time if a type is a template instantiation.
template<template<typename...> typename Tem, typename T>
struct IsInstance : public std::false_type
{
};

// Template specialization that does the magic.
template<template<typename...> typename Tem, typename... Ts>
struct IsInstance<Tem, Tem<Ts...>> : public std::true_type
{
};

template<typename T>
struct TypeInfo : public std::false_type
{
  static constexpr uint32_t id     = 0U;
  static constexpr char     name[] = "UnknownType";
};

template<size_t N, typename T>
struct TupleN
{
  template<typename... Args>
  using type = typename TupleN<N - 1, T>::template type<T, Args...>;
};

template<typename T>
struct TupleN<0, T>
{
  template<typename... Args>
  using type = std::tuple<Args...>;
};

};  // namespace types

namespace store {

struct Register
{
  std::shared_ptr<void> ptr;
  std::string           typeName;
  uint64_t              id;
  const Function*       owner;
  uint32_t              typeId;
  bool                  isDirty = true;

  std::shared_ptr<Function> ownerFunc() const;
};

uint64_t allocate(const Function* fn, uint32_t typeId, const std::string& typeName);
void     free(uint64_t id);

Register& getRegister(uint64_t id);

void markDirty(uint64_t id);

void useRegister(const Function* fn, uint64_t id);

template<typename T>
void set(uint64_t id, const std::shared_ptr<T>& data)
{
  static_assert(types::TypeInfo<T>::value, "Unknown type");
  auto& reg = getRegister(id);
  if (types::TypeInfo<T>::id == reg.typeId) {
    reg.ptr = data;
    return;
  }
  std::cerr << "Type mismatch error.\n";
  throw std::bad_alloc();
};

template<typename T>
std::shared_ptr<T> get(uint64_t id)
{
  static_assert(types::TypeInfo<T>::value, "Unknown type");
  auto& reg = getRegister(id);
  if (types::TypeInfo<T>::id == reg.typeId) {
    if (reg.isDirty) {
      auto fn = reg.ownerFunc();
      fn->run();
      reg.isDirty = false;
    }
    return std::static_pointer_cast<T>(reg.ptr);
  }
  std::cerr << "Type mismatch error.\n";
  throw std::bad_alloc();
};

void addFunction(const std::shared_ptr<Function>& fn);

};  // namespace store

namespace types {
template<size_t N>
using OutputTuple = typename TupleN<N, store::Register>::template type<>;
}

template<typename... Ts>
struct TypeList
{
  using SharedTupleType            = std::tuple<std::shared_ptr<Ts>...>;
  static constexpr size_t NumTypes = sizeof...(Ts);

  template<typename TOut>
  using FnType = std::function<TOut(std::shared_ptr<Ts>...)>;
};

template<typename TDataList, size_t N = 0>
struct RegisterAccessor
{
  static_assert(types::IsInstance<TypeList, TDataList>::value,
                "Inputs are not a type list.");
  using SharedTupleType = typename TDataList::SharedTupleType;
  using DType = typename std::tuple_element<N, SharedTupleType>::type::element_type;

  static constexpr size_t NumData = TDataList::NumTypes;

private:
  static void typeCheck(const store::Register& reg)
  {
    static_assert(types::TypeInfo<DType>::value, "Unknown type");
    if (reg.typeId != types::TypeInfo<DType>::id) {
      std::cerr << "Type mismatch error\n";
      throw std::bad_cast();
    }
  };

public:
  static void readRegisters(const std::array<uint64_t, NumData>& ids,
                            SharedTupleType&                     dst)
  {
    store::Register& reg = store::getRegister(ids[N]);
    typeCheck(reg);
    if (reg.isDirty) {
      auto fn = reg.ownerFunc();
      fn->run();
      reg.isDirty = false;
    }
    std::get<N>(dst) = std::static_pointer_cast<DType>(reg.ptr);
    if constexpr (N < TDataList::NumTypes - 1) {
      // Recurse to the next indices.
      RegisterAccessor<TDataList, N + 1>::readRegisters(ids, dst);
    }
  };

  static void writeToRegisters(const std::array<uint64_t, NumData>& ids,
                               const SharedTupleType&               src)
  {
    store::Register& reg = store::getRegister(ids[N]);
    typeCheck(reg);
    reg.ptr = std::static_pointer_cast<void>(std::get<N>(src));
    if constexpr (N < TDataList::NumTypes - 1) {
      // Recurse to the next indices.
      RegisterAccessor<TDataList, N + 1>::writeToRegisters(ids, src);
    }
  };

  static void initRegisters(const Function* fn, std::array<uint64_t, NumData>& regIds)
  {
    static_assert(types::TypeInfo<DType>::value, "Unknown type");
    regIds[N] = store::allocate(
      fn, types::TypeInfo<DType>::id, std::string(types::TypeInfo<DType>::name));
    if constexpr (N < NumData - 1) {
      RegisterAccessor<TDataList, N + 1>::initRegisters(fn, regIds);
    }
  };
};

template<typename TInputs, typename TOutputs>
struct TFunction : public Function
{
  static_assert(types::IsInstance<TypeList, TInputs>::value,
                "Inputs are not a type list.");
  static_assert(types::IsInstance<TypeList, TOutputs>::value,
                "Outputs are not a type list.");

  using InputsType                   = typename TInputs::SharedTupleType;
  using OutputsType                  = typename TOutputs::SharedTupleType;
  static constexpr size_t NumInputs  = TInputs::NumTypes;
  static constexpr size_t NumOutputs = TOutputs::NumTypes;

  using FuncType = typename TInputs::template FnType<typename TOutputs::SharedTupleType>;

private:
  std::array<uint64_t, NumOutputs> mOutputs;
  std::array<uint64_t, NumInputs>  mInputs;
  FuncType                         mFunc;

public:
  TFunction(FuncType fn, const std::array<uint64_t, NumInputs>& inputs)
      : mFunc(std::move(fn))
      , mInputs(inputs) {};

  ~TFunction()
  {
    for (auto out : mOutputs) {
      store::free(out);
    }
  };

  size_t numOutputs() const override { return NumOutputs; };

  uint64_t outputRegister(size_t index) const override
  {
    if (index < NumOutputs) {
      return mOutputs[index];
    }
    throw std::out_of_range("Index out of range");
  };

  void initOutputRegisters() override
  {
    RegisterAccessor<TOutputs>::initRegisters(this, mOutputs);
  };

  void run() override
  {
    InputsType inputs;
    RegisterAccessor<TInputs>::readRegisters(mInputs, inputs);
    OutputsType outputs = std::apply(mFunc, inputs);
    RegisterAccessor<TOutputs>::writeToRegisters(mOutputs, outputs);
  };
};

template<typename T>
struct TConstant : public Function
{
protected:
  uint64_t           mRegisterId;
  std::shared_ptr<T> mValue;

public:
  TConstant(const T& value)
      : mValue(std::make_shared<T>(value)) {};

  ~TConstant() { store::free(mRegisterId); }

  void initOutputRegisters() override
  {
    static_assert(types::TypeInfo<T>::value, "Unknown type");
    mRegisterId = store::allocate(this, types::TypeInfo<T>::id, types::TypeInfo<T>::name);
  };

  size_t numOutputs() const override { return 1; };

  uint64_t outputRegister(size_t index) const override
  {
    if (index == 0) {
      return mRegisterId;
    }
    throw std::out_of_range("Index out of range");
  };

  void run() override { store::set<T>(mRegisterId, mValue); };
};

template<typename T>
struct TVariable : public TConstant<T>
{
  void set(const T& value)
  {
    *(this->mValue) = value;
    store::markDirty(this->mRegisterId);
  };
};

namespace store {

template<typename TFunc, typename... TArgs>
std::shared_ptr<Function> makeFunction(TArgs... args)
{
  static_assert(std::is_base_of_v<Function, TFunc> &&
                  gal::func::types::IsInstance<TFunction, TFunc>::value,
                "Not a valid function type");

  auto fn = std::dynamic_pointer_cast<Function>(std::make_shared<TFunc>(args...));
  addFunction(fn);
  fn->initOutputRegisters();
  return fn;
};

template<typename T>
std::shared_ptr<Function> makeConstant(const T& value)
{
  auto fn = std::dynamic_pointer_cast<Function>(std::make_shared<TConstant<T>>(value));
  addFunction(fn);
  fn->initOutputRegisters();
  return fn;
};

}  // namespace store

namespace types {

template<size_t NMax, size_t N = 0>
void setOutputTuple(OutputTuple<NMax>& tup, const Function& fn)
{
  if constexpr (N < NMax) {
    std::get<N>(tup) = store::getRegister(fn.outputRegister(N));
  }
  if constexpr (N < NMax - 1) {
    setOutputTuple<NMax, N + 1>(tup, fn);
  }
};

template<size_t N>
OutputTuple<N> makeOutputTuple(const Function& fn)
{
  OutputTuple<N> tup;
  setOutputTuple<N>(tup, fn);
  return tup;
};

}  // namespace types

template<typename T>
types::OutputTuple<1> constant(const T& value)
{
  auto fn = store::makeConstant<T>(value);
  return types::makeOutputTuple<1>(*fn);
};

}  // namespace func
}  // namespace gal

namespace std {
std::ostream& operator<<(std::ostream& ostr, const gal::func::store::Register& reg);
}

#define TYPE_INFO(type, idInt)                                    \
  template<>                                                      \
  struct gal::func::types::TypeInfo<type> : public std::true_type \
  {                                                               \
    static constexpr uint32_t id     = idInt;                     \
    static constexpr char     name[] = #type;                     \
  };

#define GAL_CONCAT(x, y) x##y

#define _GAL_ARG_TYPE(type, name, desc) type
#define GAL_ARG_TYPE(argTuple) _GAL_ARG_TYPE argTuple

#define _GAL_ARG_NAME(type, name, desc) name
#define GAL_ARG_NAME(argTuple) _GAL_ARG_NAME argTuple

#define _GAL_EXPAND_TYPE_TUPLE(...) MAP_LIST(GAL_ARG_TYPE, __VA_ARGS__)
#define GAL_EXPAND_TYPE_TUPLE(types) _GAL_EXPAND_TYPE_TUPLE types

#define GAL_EXPAND_SHARED_ARG(argTuple) \
  std::shared_ptr<GAL_ARG_TYPE(argTuple)> GAL_ARG_NAME(argTuple)

#define GAL_EXPAND_SHARED_ARGS(...) MAP_LIST(GAL_EXPAND_SHARED_ARG, __VA_ARGS__)

#define GAL_REGISTER_ARG(typeTuple) \
  const gal::func::store::Register& GAL_ARG_NAME(typeTuple)

#define GAL_EXPAND_REGISTER_ARGS(...) MAP_LIST(GAL_REGISTER_ARG, __VA_ARGS__)

#define GAL_FN_IMPL_NAME(fnName) GAL_CONCAT(fnName, _impl)

// clang-format off
// clang-format on
#define GAL_FUNC_DECL(outTypes, fnName, hasArgs, nArgs, fnDesc, ...)                                  \
  gal::func::TypeList<GAL_EXPAND_TYPE_TUPLE(outTypes)>::SharedTupleType                               \
                                       GAL_FN_IMPL_NAME(fnName)(GAL_EXPAND_SHARED_ARGS(__VA_ARGS__)); \
  gal::func::types::OutputTuple<nArgs> fnName(GAL_EXPAND_REGISTER_ARGS(__VA_ARGS__));

TYPE_INFO(bool, 0x9566a7b1);
TYPE_INFO(int32_t, 0x9234a3b1);
TYPE_INFO(float, 0x32542672);
TYPE_INFO(std::string, 0x12340989);