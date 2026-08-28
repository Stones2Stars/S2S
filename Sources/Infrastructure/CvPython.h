#pragma once

#ifndef CvPython_h__
#define CvPython_h__

#include "Tools/FProfiler.h"

#include <boost155/type_traits.hpp>
#include <boost155/utility/enable_if.hpp>
#include <boost/python/list.hpp>

#include "Tools/FAssert.h"
#include "Defines/CvString.h"
#include "Python/CyArgsList.h"
#include "Defines/CvGlobals.h"

#define FPythonAssert(expr, moduleName, functionName) FAssertMsg(expr, CvString::format("%s.%s", moduleName, functionName).c_str())

#define DECLARE_PY_WRAPPER(_wrapper, _class)			\
namespace Cy											\
{														\
	template <>											\
	struct ArgTraits< typename Cy::base_type<_class>::type > \
	{													\
		template < class Ty_ >							\
		static void add(Args& args, Ty_ arg)			\
		{												\
			args.add_wrapped(_wrapper(arg));			\
		}												\
	};													\
}

//
//	The object crosses to Python as its (owner, id) IDENTITY rather than as a Cy* handle -- see Cy::PyIdentity.
//	⛔ Declared ONCE per class, so EVERY push site converts together (docs/architecture/patterns.md §DRY (single implementation)): the event
//	reports, the outcome callbacks, anything doing `Cy::Args() << pObject`. A per-site conversion would leave the
//	two representations live at the same time, which is the half-migration this closes.
//	⚠ The `class_<Cy*>` REGISTRATION stays regardless: CvGameObject::createPythonWrapper builds those handles on
//	its own path and still needs the converter (patterns.md -- registration is not binding).
//
#define DECLARE_PY_IDENTITY(_class, _ownerOf, _idOf)		\
namespace Cy											\
{														\
	template <>											\
	struct ArgTraits< typename Cy::base_type<_class>::type > \
	{													\
		template < class Ty_ >							\
		static void add(Args& args, Ty_ arg)			\
		{												\
			args.add_identity(arg ? (int)(arg->_ownerOf) : -1, \
			                  arg ? (int)(arg->_idOf)    : -1); \
		}												\
	};													\
}

#define DECLARE_PY_WRAPPED(_class)						\
namespace Cy											\
{														\
	template <>											\
	struct ArgTraits< typename Cy::base_type<_class>::type > \
	{													\
		template < class Ty_ >							\
		static void add(Args& args, Ty_ arg)			\
		{												\
			args.add_wrapped(arg);						\
		}												\
	};													\
}

namespace Cy
{
	template <class Item_>
	const boost::python::list makeList(const std::vector<Item_>& vector)
	{
		PROFILE_EXTRA_FUNC();
		python::list list = python::list();
		foreach_(const Item_& i, vector)
			list.append((int)i);
		return list;
	}

	template < class Ty_ >
	struct base_type
	{
		typedef typename bst::remove_cv<
			typename bst::remove_reference<
			typename bst::remove_pointer<Ty_>::type
			>::type
		>::type type;
	};
	// Wrapper for simple array arguments
	//template < class Ty_ >
	//struct Array
	//{
	//	typedef Ty_ value_type;
	//
	//	Array(const value_type* vals, int len) : vals(vals), len(len) {}
	//
	//	const value_type* vals;
	//	int len;
	//};

	struct Args;

	//// Base class for Arg type traits.
	//// Specialize this to provide special behaviour for certain types when they are
	//// passed to Python.
	//template < class Ty_ >
	//struct ArgTraitsBase
	//{
	//	//typedef Ty_ value_type;
	//};

	// Default implementation
	template < class Ty_ >
	struct ArgTraits //: ArgTraitsBase<Ty_>
	{
		template < class RealTy_ >
		static void add(Args& args, const RealTy_& arg) { args.args().add(arg); }
	};

	// Array specialization
	//template < class Ty_ >
	//struct ArgTraits< Array<Ty_> > //: ArgTraitsBase< Array<Ty_> >
	//{
	//	static void add(Args& args, const Array<Ty_>& arg)
	//	{
	//		args.args().add(arg.vals, arg.len);
	//	}
	//};

	// vector specialization
	template < class Ty_ >
	struct ArgTraits< std::vector<Ty_> > //: ArgTraitsBase< std::vector<Ty_> >
	{
		static void add(Args& args, const std::vector<Ty_>& arg)
		{
			args.args().add(arg.empty() ? NULL : &arg[0], arg.size());
		}
	};

	// Lifetime management for objects passed as arguments
	struct PyWrapBase
	{
		virtual ~PyWrapBase() {};
		PyObject* pyobj;
	};

	// Default implementation, matches all classes
	template < class Ty_ >
	struct PyWrap : PyWrapBase
	{
		STATIC_ASSERT(bst::is_class<Ty_>::value, Wrong_type_for_PyWrap);

		typedef Ty_ value_type;
		explicit PyWrap(const value_type& obj) : obj(obj)
		{
			pyobj = gDLL->getPythonIFace()->makePythonObject(&(this->obj));
		}

		value_type obj;
	};

	//
	//	A game object's IDENTITY, crossing as the (owner, id) pair the read library is addressed by.
	//
	//	⛔ This is what a callback hands over instead of a Cy* HANDLE. The read surfaces (the object's own accessor / CyEnabler)
	//	are deliberately ID-BASED so the legacy per-type wrappers can be cut, and the cut leaves those wrappers
	//	carrying ZERO defs -- so a script handed one can ask it nothing, not even which object it is. Pushing the
	//	pair directly is what makes the KEPT engine->Python direction usable at all.
	//	⚑ It occupies ONE argument slot, so an event's existing argument POSITIONS are unchanged and only a
	//	handler that actually uses the object is touched.
	//	⚠ Holds the tuple for the duration of the call -- pyobj is a BORROWED pointer into it, exactly as PyWrap
	//	borrows from the object it owns.
	//
	struct PyIdentity : PyWrapBase
	{
		//	⛔ CyArgsList OWNS what it is handed. Every other add() pushes a FRESH object -- PyInt_FromLong,
		//	PyString_FromString, PyUnicode_FromWideChar all return a NEW reference -- so the list releases what it
		//	holds. Handing it `identity.ptr()` bare would hand it a BORROWED pointer that `identity` still owns,
		//	and the tuple would be released twice: freed while this object still points at it, after which the
		//	interpreter reads freed memory on some later touch. So incref, and give the list its own reference.
		//	⚠ The failure is REMOTE from the cause -- an access violation inside python24 at whatever next walks
		//	the object -- so it reads as an interpreter bug rather than as a refcount error here.
		//	⛔ THE OWNERSHIP CONTRACT IS "CREATE AND HAND OVER", and it is set by PyWrap beside this: PyWrap builds
		//	its object with makePythonObject, stores the pointer, and NEVER decrefs -- a path that has worked for
		//	years, so the args list takes ownership of what it is given. Every other CyArgsList::add agrees:
		//	PyInt_FromLong / PyString_FromString / PyUnicode_FromWideChar all hand over a FRESH reference.
		//	⚠ So handing it `identity.ptr()` bare would give ONE reference TWO owners -- this member and the list.
		//	The incref is what makes the count match the number of owners; a decref in a destructor here would be
		//	the same error mirrored, releasing what the list already owns.
		PyIdentity(int iOwner, int iId) : identity(python::make_tuple(iOwner, iId))
		{
			pyobj = python::incref(identity.ptr());
		}

		python::tuple identity;
	};

	//
	// type for input args to python functions
	//
	struct Args
	{
	public:
		Args() {}
		template < class Arg0_ >
		explicit Args(const Arg0_& arg0) { add(arg0); }
		template < class Arg0_, class Arg1_ >
		explicit Args(const Arg0_& arg0, const Arg1_& arg1) { add(arg0); add(arg1); }
		template < class Arg0_, class Arg1_, class Arg2_ >
		explicit Args(const Arg0_& arg0, const Arg1_& arg1, const Arg2_& arg2) { add(arg0); add(arg1); add(arg2); }

		template < class Ty_ >
		Args& add(const Ty_& arg)
		{
			ArgTraits<
				typename Cy::base_type<Ty_>::type
			>::add(*this, arg);
			return *this;
		}

		//template < class Ty_ >
		//Args& add(const std::vector<Ty_>& arr) { add(arr.empty()? NULL : &arr[0], arr.size()); return *this; }

		template < class Ty_ >
		Args& add_wrapped(const Ty_& arg)
		{
			m_wrapped.push_back(bst::shared_ptr<PyWrapBase>(new PyWrap<Ty_>(arg)));
			m_args.add(m_wrapped.back()->pyobj);
			return *this;
		}

		// The (owner, id) pair a callback hands over in place of a Cy* handle -- see PyIdentity above. Same
		// lifetime discipline as add_wrapped: the holder lives in m_wrapped until the call has been made.
		Args& add_identity(int iOwner, int iId)
		{
			m_wrapped.push_back(bst::shared_ptr<PyWrapBase>(new PyIdentity(iOwner, iId)));
			m_args.add(m_wrapped.back()->pyobj);
			return *this;
		}

		//template < class Ty_ >
		//typename bst::enable_if<
		//	bst::type_traits::ice_or<
		//	!bst::is_class<Ty_>::value,
		//	bst::is_pointer<Ty_>::value
		//	>,
		//	Args&
		//>::type add(const Ty_& arg)
		//{
		//	m_args.add(arg);
		//	return *this;
		//}

		template < class Ty_ > friend struct ArgTraits;

		template < class Ty_ >
		Args& operator<<(const Ty_& arg)
		{
			return add(arg);
		}

		void* makeFunctionArgs() const
		{
			// Need to cast off const here as the function that is eventually called is an EXE one so we can't change the signature (maybe const isn't actually in a function sig, but whatever).
			return const_cast<CyArgsList&>(m_args).makeFunctionArgs();
		}

		const CyArgsList& args() const { return m_args; }
		CyArgsList& args() { return m_args; }

	private:
		CyArgsList m_args;
		std::vector< bst::shared_ptr< PyWrapBase > > m_wrapped;
	};

	// These classes allows mappings between C++ types and python types to be defined for
	// Python function return values.
	template < class DesiredTy_, class PyTy_ >
	struct PythonReturnVarMappingBase
	{
		typedef DesiredTy_ desired_type;
		static DesiredTy_ default_value;

		template <class Ty_, bool b>	struct EnumDetect				{ typedef Ty_ type; };
		template <class Ty_>			struct EnumDetect<Ty_, true>	{ typedef long type; };

		typedef typename EnumDetect<PyTy_, bst::is_enum<DesiredTy_>::value>::type py_type;
		static desired_type convert(const py_type& pyVal) { return static_cast<desired_type>(pyVal); }
	};

	template < class DesiredTy_, class PyTy_ >
	DesiredTy_ Cy::PythonReturnVarMappingBase<DesiredTy_, PyTy_>::default_value = DesiredTy_();

	// By default all types map to themselves
	template < class DesiredTy_ >
	struct PythonReturnVarMapping : PythonReturnVarMappingBase<DesiredTy_, DesiredTy_> {};
	// bool maps to a long
	template <>
	struct PythonReturnVarMapping<bool> : PythonReturnVarMappingBase<bool, long> {};
	// int maps to a long
	template <>
	struct PythonReturnVarMapping<int> : PythonReturnVarMappingBase<int, long> {};

	// NO RETURN VALUE, NO ARGUMENTS ============================================================================
	inline void call(const char* const moduleName, const char* const functionName, bool* bSucceeded = NULL)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		bool bOK = gDLL->getPythonIFace()->callFunction(moduleName, functionName);
		FPythonAssert(bOK, moduleName, functionName);
		if (bSucceeded != NULL)
		{
			*bSucceeded = bOK;
		}
	}

	// Return call success, no assert
	inline bool call_optional(const char* const moduleName, const char* const functionName)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		return gDLL->getPythonIFace()->callFunction(moduleName, functionName);
	}

	// Check for success and default impl flag
	inline bool call_override(const char* const moduleName, const char* const functionName)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		return gDLL->getPythonIFace()->callFunction(moduleName, functionName)
			&& !gDLL->getPythonIFace()->pythonUsingDefaultImpl();
	}

	// RETURN VALUE, NO ARGUMENTS ============================================================================
	template < class ReturnValueTy_ >
	inline ReturnValueTy_ call(const char* const moduleName, const char* const functionName, bool* bSucceeded = NULL)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		PythonReturnVarMapping<ReturnValueTy_>::py_type rvalPy = PythonReturnVarMapping<ReturnValueTy_>::default_value;
		bool bOK = gDLL->getPythonIFace()->callFunction(moduleName, functionName, NULL, &rvalPy);
		FPythonAssert(bOK, moduleName, functionName);
		if (bSucceeded != NULL)
		{
			*bSucceeded = bOK;
		}
		return PythonReturnVarMapping<ReturnValueTy_>::convert(rvalPy);
	}

	// Return call success, no assert
	template < class ReturnValueTy_ >
	inline bool call_optional(const char* const moduleName, const char* const functionName, ReturnValueTy_& rval)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		PythonReturnVarMapping<ReturnValueTy_>::py_type rvalPy;
		if (gDLL->getPythonIFace()->callFunction(moduleName, functionName, NULL, &rvalPy))
		{
			rval = PythonReturnVarMapping<ReturnValueTy_>::convert(rvalPy);
			return true;
		}
		return false;
	}

	// Check for success and default impl flag
	template < class ReturnValueTy_ >
	// inline
	// Make sure to disallow Cy::Args matching ReturnValueTy_
	typename bst::enable_if_c<
		!bst::is_convertible<
			typename bst::remove_cv<ReturnValueTy_>::type,
			Cy::Args
		>::value,
		bool
	>::type call_override(const char* const moduleName, const char* const functionName, ReturnValueTy_& rval)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		PythonReturnVarMapping<ReturnValueTy_>::py_type rvalPy;
		if (gDLL->getPythonIFace()->callFunction(moduleName, functionName, NULL, &rvalPy)
			&& !gDLL->getPythonIFace()->pythonUsingDefaultImpl())
		{
			rval = PythonReturnVarMapping<ReturnValueTy_>::convert(rvalPy);
			return true;
		}
		return false;
	}

	// NO RETURN VALUE, ARGUMENTS ============================================================================
	inline void call(const char* const moduleName, const char* const functionName, const Cy::Args& args, bool* bSucceeded = NULL)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		bool bOK = gDLL->getPythonIFace()->callFunction(moduleName, functionName, args.makeFunctionArgs());
		FPythonAssert(bOK, moduleName, functionName);
		if (bSucceeded != NULL)
		{
			*bSucceeded = bOK;
		}
	}

	inline bool call_optional(const char* const moduleName, const char* const functionName, const Cy::Args& args)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		return gDLL->getPythonIFace()->callFunction(moduleName, functionName, args.makeFunctionArgs());
	}

	// Check for success and default impl flag
	inline bool call_override(const char* const moduleName, const char* const functionName, const Cy::Args& args)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		return gDLL->getPythonIFace()->callFunction(moduleName, functionName, args.makeFunctionArgs())
			&& !gDLL->getPythonIFace()->pythonUsingDefaultImpl();
	}

	// RETURN VALUE, ARGUMENTS ============================================================================
	template < class ReturnValueTy_ >
	inline ReturnValueTy_ call(const char* const moduleName, const char* const functionName, const Cy::Args& args, bool* bSucceeded = NULL)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		PythonReturnVarMapping<ReturnValueTy_>::py_type rvalPy = PythonReturnVarMapping<ReturnValueTy_>::default_value;
		bool bOK = gDLL->getPythonIFace()->callFunction(moduleName, functionName, args.makeFunctionArgs(), &rvalPy);
		FPythonAssert(bOK, moduleName, functionName);
		if (bSucceeded != NULL)
		{
			*bSucceeded = bOK;
		}
		return PythonReturnVarMapping<ReturnValueTy_>::convert(rvalPy);
	}

	template < class ReturnValueTy_ >
	inline bool call_optional(const char* const moduleName, const char* const functionName, const Cy::Args& args, ReturnValueTy_& rval)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		PythonReturnVarMapping<ReturnValueTy_>::py_type rvalPy;
		if (gDLL->getPythonIFace()->callFunction(moduleName, functionName, args.makeFunctionArgs(), &rvalPy))
		{
			rval = PythonReturnVarMapping<ReturnValueTy_>::convert(rvalPy);
			return true;
		}
		return false;
	}

	// Check for success and default impl flag
	template < class ReturnValueTy_ >
	inline bool call_override(const char* const moduleName, const char* const functionName, const Cy::Args& args, ReturnValueTy_& rval)
	{
		FPythonAssert(gDLL->getPythonIFace()->isInitialized(), moduleName, functionName);
		PythonReturnVarMapping<ReturnValueTy_>::py_type rvalPy;
		if (gDLL->getPythonIFace()->callFunction(moduleName, functionName, args.makeFunctionArgs(), &rvalPy)
			&& !gDLL->getPythonIFace()->pythonUsingDefaultImpl())
		{
			rval = PythonReturnVarMapping<ReturnValueTy_>::convert(rvalPy);
			return true;
		}
		return false;
	}

	struct StackFrame
	{
		CvString filename;
		int line;
		CvString code;

		StackFrame() {}
		StackFrame(const CvString& filename, int line, const CvString& code) : filename(filename), line(line), code(code) {}
	};

	std::vector<StackFrame> get_stack_trace();

	namespace call_policy
	{
		template <typename T>
		void registerAllowPyIntAsType()
		{
			python::converter::registry::push_back(&PyIntConverter::convertible, &PyIntConverter::fromPython<T>, python::type_id<T>());
			python::converter::registry::insert(PyIntConverter::toPython, python::type_id<T>());
		}

		const struct PyIntConverter
		{
			static void* convertible(PyObject* obj)
			{
				return PyInt_Check(obj) ? obj : nullptr;
			}

			template <typename T>
			static void fromPython(PyObject* obj, boost::python::converter::rvalue_from_python_stage1_data* data)
			{
				void* const storage = reinterpret_cast<python::converter::rvalue_from_python_storage<int>*>(data)->storage.bytes;
				new (storage) T(static_cast<T>(PyInt_AS_LONG(obj)));
				data->convertible = storage;
			}

			static PyObject* toPython(const void* p)
			{
				return PyInt_FromLong(*static_cast<const long*>(p));
			}
		};
	}
}

#endif // CvPython_h__
