#pragma once

#ifndef CvInfoUtil_h__
#define CvInfoUtil_h__

#include "Tools/FProfiler.h"

#include "CvGameCoreDLL.h"
#include "Defines/CvGlobals.h"
#include "CvInfoClassTraits.h"
#include "Infrastructure/CvXMLLoadUtility.h"
#include "Tools/CheckSum.h"
#include "Infrastructure/IDValueMap.h"
#include "Infrastructure/BoolExpr.h"
#include "Engine/CvPropertyManipulators.h"

struct WrappedVar;


struct CvInfoUtil
	: private bst::noncopyable
{
	template <class CvInfoClass_T>
	CvInfoUtil(const CvInfoClass_T* info)
		: m_eInfoClass(InfoClassTraits<CvInfoClass_T>::InfoClassEnum)
		, m_bForceImmediate(false)
	{
		const_cast<CvInfoClass_T*>(info)->getDataMembers(*this);
	}

	// Struct-element overload: a nested struct (e.g. a row of a struct-vector) declares its fields
	// the same way an info does, but its FK delayed-resolution decisions belong to the OWNING info
	// class, which is passed explicitly here rather than derived from the struct type. Nested FKs
	// always resolve immediately (m_bForceImmediate): the struct lives in a growable container, so a
	// raw pointer into it can't be safely held across the parse phase for deferred resolution.
	template <class Struct_T>
	CvInfoUtil(InfoClassTypes eOwnerClass, Struct_T* obj)
		: m_eInfoClass(eOwnerClass)
		, m_bForceImmediate(true)
	{
		obj->getDataMembers(*this);
	}

	~CvInfoUtil()
	{
		PROFILE_EXTRA_FUNC();
		foreach_(const WrappedVar* wrapper, m_wrappedVars)
			delete wrapper;
	}

	void initDataMembers()
	{
		PROFILE_EXTRA_FUNC();
		foreach_(WrappedVar* wrapper, m_wrappedVars)
			wrapper->initVar();
	}

	void uninitDataMembers()
	{
		PROFILE_EXTRA_FUNC();
		foreach_(WrappedVar* wrapper, m_wrappedVars)
			wrapper->uninitVar();
	}

	void checkSum(uint32_t& iSum) const
	{
		PROFILE_EXTRA_FUNC();
		foreach_(const WrappedVar* wrapper, m_wrappedVars)
			wrapper->checkSum(iSum);
	}

	void readXml(CvXMLLoadUtility* pXML)
	{
		PROFILE_EXTRA_FUNC();
		foreach_(WrappedVar* wrapper, m_wrappedVars)
			wrapper->readXml(pXML);
	}

	// Parse-then-link: resolve this object's deferred FK columns (captured during readXml as
	// <Type>-string refs in the global delayed-resolution map). No-op for fields that resolved
	// immediately. Driven per-table by InfoTable<T>::link().
	void link()
	{
		PROFILE_EXTRA_FUNC();
		foreach_(WrappedVar* wrapper, m_wrappedVars)
			wrapper->link();
	}

	void copyNonDefaults(const CvInfoUtil otherUtil)
	{
		PROFILE_EXTRA_FUNC();
		for (uint32_t i = 0, num = m_wrappedVars.size(); i < num; i++)
			m_wrappedVars[i]->copyNonDefaults(otherUtil.m_wrappedVars[i]);
	}

	///=============================
	/// Variable wrapper base class
	///=============================

	struct WrappedVar : bst::noncopyable
	{
		friend struct CvInfoUtil;

	protected:
		WrappedVar(void* var, const wchar_t* tag)
			: m_ptr(var)
			, m_tag(tag)
		{}

		virtual ~WrappedVar() {}

		virtual void initVar() {}
		virtual void uninitVar() {}
		// Parse-then-link hook: default no-op. Only FK wrappers that can defer override it to
		// resolve their captured <Type>-string from the global delayed-resolution map.
		virtual void link() {}
		//virtual void copyVar() {}

		//virtual void read(FDataStreamBase*) {}
		//virtual void write(FDataStreamBase*) {}

		virtual void checkSum(uint32_t&) const = 0;
		virtual void readXml(CvXMLLoadUtility*) = 0;
		virtual void copyNonDefaults(const WrappedVar*)	= 0;

		void* m_ptr;
		const std::wstring m_tag;
	};

	///=================
	/// Integer wrapper
	///=================

	template <typename T>
	struct IntWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		IntWrapper(T& var, const wchar_t* tag, T defaultValue)
			: WrappedVar(static_cast<void*>(&var), tag)
			, m_default(defaultValue)
		{}

		void initVar()
		{
			ref() = m_default;
		}

		void checkSum(uint32_t& iSum) const
		{
			CheckSum(iSum, ref());
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			// Pass the wrapper's default - the loader assigns its default param when the tag
			// is absent, so omitting it would stomp a non-zero default with 0.
			pXML->GetOptionalChildXmlValByName(&ref(), m_tag.c_str(), m_default);
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			if (ref() == m_default)
				ref() = static_cast<const IntWrapper*>(source)->ref();
		}

		T& ref() const { return *static_cast<T*>(m_ptr); }

	private:
		const T m_default;
	};

	CvInfoUtil& add(int& var, const wchar_t* tag, int defaultValue = 0)
	{
		m_wrappedVars.push_back(new IntWrapper<int>(var, tag, defaultValue));
		return *this;
	}

	CvInfoUtil& add(bool& var, const wchar_t* tag, bool defaultValue = false)
	{
		m_wrappedVars.push_back(new IntWrapper<bool>(var, tag, defaultValue));
		return *this;
	}

	CvInfoUtil& add(float& var, const wchar_t* tag, float defaultValue = 0.0f)
	{
		m_wrappedVars.push_back(new IntWrapper<float>(var, tag, defaultValue));
		return *this;
	}

	///==============
	/// Enum wrapper
	///==============

	template <typename Enum_t>
	struct EnumWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		EnumWrapper(Enum_t& var, const wchar_t* tag)
			: WrappedVar(static_cast<void*>(&var), tag)
		{}

		void initVar()
		{
			ref() = static_cast<Enum_t>(-1);
		}

		void checkSum(uint32_t& iSum) const
		{
			CheckSum(iSum, ref());
		}

		virtual void readXml(CvXMLLoadUtility* pXML)
		{
			CvXMLLoadUtility::SetOptionalInfoType<NO_DELAYED_RESOLUTION>(pXML, ref(), m_tag.c_str());
		}

		virtual void copyNonDefaults(const WrappedVar* source)
		{
			if (ref() == -1)
				ref() = static_cast<const EnumWrapper*>(source)->ref();
		}

		// Resolve a deferred ref captured in the global delayed-resolution map (set by the delayed
		// readXml path). No-op when this field resolved immediately.
		virtual void link()
		{
			int* pField = reinterpret_cast<int*>(&ref());
			const CvString* pszType = GC.getDelayedResolution(pField);
			if (pszType != NULL)
			{
				ref() = static_cast<Enum_t>(GC.getInfoTypeForString(*pszType));
				GC.removeDelayedResolution(pField);
			}
		}

		Enum_t& ref() const { return *static_cast<Enum_t*>(m_ptr); }
	};

	///======================================
	/// Enum with delayed resolution wrapper
	///======================================

	template <typename Enum_t>
	struct EnumWithDelayedResolutionWrapper : EnumWrapper<Enum_t>
	{
		friend struct CvInfoUtil;

	protected:
		EnumWithDelayedResolutionWrapper(Enum_t& var, const wchar_t* tag)
			: EnumWrapper<Enum_t>(var, tag)
		{}

		void uninitVar()
		{
			GC.removeDelayedResolution(reinterpret_cast<int*>(&ref()));
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			CvXMLLoadUtility::SetOptionalInfoType<USE_DELAYED_RESOLUTION>(pXML, ref(), m_tag.c_str());
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			GC.copyNonDefaultDelayedResolution(reinterpret_cast<int*>(&ref()), reinterpret_cast<int*>(&(static_cast<const EnumWithDelayedResolutionWrapper*>(source)->ref())));
		}
	};

	template <typename Enum_t>
	CvInfoUtil& addEnum(Enum_t& var, const wchar_t* tag)
	{
		if (!m_bForceImmediate && m_eInfoClass > NO_INFO_CLASS && GC.isDelayedResolutionRequired(m_eInfoClass, InfoClassTraits<Enum_t>::InfoClassEnum))
			m_wrappedVars.push_back(new EnumWithDelayedResolutionWrapper<Enum_t>(var, tag));
		else
			m_wrappedVars.push_back(new EnumWrapper<Enum_t>(var, tag));
		return *this;
	}

	///==========================
	/// Enum-as-int FK wrapper
	///==========================
	/// For legacy members typed plain `int` that actually hold a foreign-key type index resolved at read
	/// time via GetInfoClass (e.g. a ColorTypes index stored in an int). Reads the <tag> type string and
	/// resolves it immediately (no delayed resolution); default -1. Use addEnum instead whenever the
	/// member is a real enum type — this exists only for the int-typed legacy fields.

	struct IntInfoClassWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		IntInfoClassWrapper(int& var, const wchar_t* tag)
			: WrappedVar(static_cast<void*>(&var), tag)
		{}

		int& ref() const { return *static_cast<int*>(m_ptr); }

		void initVar() { ref() = -1; }

		void checkSum(uint32_t& iSum) const { CheckSum(iSum, ref()); }

		void readXml(CvXMLLoadUtility* pXML)
		{
			CvString szTextVal;
			if (pXML->GetOptionalChildXmlValByName(szTextVal, m_tag.c_str()))
				ref() = pXML->GetInfoClass(szTextVal);
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			if (ref() == -1)
				ref() = static_cast<const IntInfoClassWrapper*>(source)->ref();
		}
	};

	CvInfoUtil& addEnumAsInt(int& var, const wchar_t* tag)
	{
		m_wrappedVars.push_back(new IntInfoClassWrapper(var, tag));
		return *this;
	}

	///================
	/// String wrapper
	///================

	struct StringWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		StringWrapper(CvString& var, const wchar_t* tag)
			: WrappedVar(static_cast<void*>(&var), tag)
		{}

		void checkSum(uint32_t&) const
		{
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			pXML->GetOptionalChildXmlValByName(ref(), m_tag.c_str());
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			if (ref().empty() || ref() == CvString::format("").c_str())
				ref() = static_cast<const StringWrapper*>(source)->ref();
		}

		CvString& ref() const { return *static_cast<CvString*>(m_ptr); }
	};

	CvInfoUtil& add(CvString& var, const wchar_t* tag)
	{
		m_wrappedVars.push_back(new StringWrapper(var, tag));
		return *this;
	}

	///==========================
	/// Fixed char-array wrapper
	///==========================
	/// A `char[N]` buffer filled by GetOptionalChildXmlValByName from <tag> (e.g. the small fixed model
	/// strings on the *ModelInfo classes). The member stays char[N] (getters unchanged); absent tag =>
	/// left at its default (empty string). The array size N is captured from the member's type.

	struct FixedCharArrayWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		FixedCharArrayWrapper(char* var, const wchar_t* tag, int size)
			: WrappedVar(static_cast<void*>(var), tag)
			, m_size(size)
		{}

		char* ref() const { return static_cast<char*>(m_ptr); }

		void initVar() { ref()[0] = '\0'; }

		void checkSum(uint32_t& iSum) const
		{
			for (int i = 0; i < m_size && ref()[i] != '\0'; i++)
				CheckSum(iSum, ref()[i]);
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			pXML->GetOptionalChildXmlValByName(ref(), m_tag.c_str());
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			if (ref()[0] == '\0')
			{
				const char* src = static_cast<const FixedCharArrayWrapper*>(source)->ref();
				int i = 0;
				for (; i < m_size - 1 && src[i] != '\0'; i++)
					ref()[i] = src[i];
				ref()[i] = '\0';
			}
		}

	private:
		const int m_size;
	};

	template <int N>
	CvInfoUtil& add(char (&var)[N], const wchar_t* tag)
	{
		m_wrappedVars.push_back(new FixedCharArrayWrapper(var, tag, N));
		return *this;
	}

	///================
	/// Vector wrapper
	///================

	template <typename T>
	struct VectorWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		VectorWrapper(std::vector<T>& var, const wchar_t* tag)
			: WrappedVar(static_cast<void*>(&var), tag)
		{}

		void checkSum(uint32_t& iSum) const
		{
			CheckSumC(iSum, ref());
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			pXML->SetOptionalVector(&ref(), m_tag.c_str());
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			foreach_(const T& element, static_cast<const VectorWrapper*>(source)->ref())
				if (element > -1 && algo::none_of_equal(ref(), element))
					ref().push_back(element);
			algo::sort(ref());
		}

		std::vector<T>& ref() const { return *static_cast<std::vector<T>*>(m_ptr); }
	};

	template <typename T>
	CvInfoUtil& add(std::vector<T>& vec, const wchar_t* tag)
	{
		m_wrappedVars.push_back(new VectorWrapper<T>(vec, tag));
		return *this;
	}

	///====================
	/// IDValueMap wrapper
	///====================

	template <typename IDValueMap_T>
	struct IDValueMapWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		IDValueMapWrapper(IDValueMap_T& var, const wchar_t* tag)
			: WrappedVar(static_cast<void*>(&var), tag)
		{}

		void checkSum(uint32_t& iSum) const
		{
			CheckSumC(iSum, ref());
		}

		virtual void readXml(CvXMLLoadUtility* pXML)
		{
			ref().read(pXML, m_tag.c_str());
		}

		virtual void copyNonDefaults(const WrappedVar* source)
		{
			ref().copyNonDefaults(static_cast<const IDValueMapWrapper*>(source)->ref());
		}

		IDValueMap_T& ref() const { return *static_cast<IDValueMap_T*>(m_ptr); }
	};

	///============================================
	/// IDValueMap with delayed resolution wrapper
	///============================================

	template <typename IDValueMap_T>
	struct IDValueMapWithDelayedResolutionWrapper
		: public IDValueMapWrapper<IDValueMap_T>
	{
		friend struct CvInfoUtil;

	protected:
		IDValueMapWithDelayedResolutionWrapper(IDValueMap_T& var, const wchar_t* tag)
			: IDValueMapWrapper<IDValueMap_T>(var, tag)
		{}

		void uninitVar()
		{
			ref().removeDelayedResolution();
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			ref().readWithDelayedResolution(pXML, m_tag.c_str());
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			ref().copyNonDefaultDelayedResolution(static_cast<const IDValueMapWithDelayedResolutionWrapper*>(source)->ref());
		}
	};

	template <typename T1, int default_>
	CvInfoUtil& add(IDValueMap<T1, int, default_>& map, const wchar_t* rootTag)
	{
		if (GC.isDelayedResolutionRequired(m_eInfoClass, InfoClassTraits<T1>::InfoClassEnum))
			m_wrappedVars.push_back(new IDValueMapWithDelayedResolutionWrapper<IDValueMap<T1, int, default_> >(map, rootTag));
		else
			m_wrappedVars.push_back(new IDValueMapWrapper<IDValueMap<T1, int, default_> >(map, rootTag));
		return *this;
	}

	///=====================================
	/// IDValueMap of paired arrays wrapper
	///=====================================

	template <typename IDValueMap_T, DelayedResolutionTypes delayedRes_>
	struct IDValueMapOfPairedArrayWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		IDValueMapOfPairedArrayWrapper(IDValueMap_T& var, const wchar_t* rootTag, const wchar_t* firstChildTag, const wchar_t* secondChildTag)
			: WrappedVar(static_cast<void*>(&var), rootTag)
			, m_firstChildTag(firstChildTag)
			, m_secondChildTag(secondChildTag)
		{}

		void uninitVar()
		{
			if (delayedRes_ == USE_DELAYED_RESOLUTION)
				ref().removeDelayedResolution();
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			ref()._readPairedArrays<delayedRes_>(pXML, m_tag.c_str(), m_firstChildTag.c_str(), m_secondChildTag.c_str());
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			ref().copyNonDefaultPairedArrays(static_cast<const IDValueMapOfPairedArrayWrapper*>(source)->ref());
		}

		void checkSum(uint32_t& iSum) const
		{
			CheckSumC(iSum, ref());
		}

		IDValueMap_T& ref() const { return *static_cast<IDValueMap_T*>(m_ptr); }

	private:
		const std::wstring m_firstChildTag;
		const std::wstring m_secondChildTag;
	};

	template <typename T1, size_t arraySize_, int default_>
	CvInfoUtil& add(IDValueMap<T1, bst::array<int, arraySize_>, default_>& map, const wchar_t* rootTag, const wchar_t* firstChildTag, const wchar_t* secondChildTag)
	{
		if (GC.isDelayedResolutionRequired(m_eInfoClass, InfoClassTraits<T1>::InfoClassEnum))
			m_wrappedVars.push_back(new IDValueMapOfPairedArrayWrapper<IDValueMap<T1, bst::array<int, arraySize_>, default_>, USE_DELAYED_RESOLUTION>(map, rootTag, firstChildTag, secondChildTag));
		else
			m_wrappedVars.push_back(new IDValueMapOfPairedArrayWrapper<IDValueMap<T1, bst::array<int, arraySize_>, default_>, NO_DELAYED_RESOLUTION>(map, rootTag, firstChildTag, secondChildTag));
		return *this;
	}

	///=====================
	/// Struct-vector wrapper
	///=====================
	/// For std::vector<Struct_T> serialized as <rootTag><elemTag>..fields..</elemTag>...</rootTag>,
	/// where Struct_T exposes getDataMembers(CvInfoUtil&) just like an info class. Each element is
	/// read/checksummed/merged by recursing the same declarative machinery (using the owning info's
	/// class for delayed-resolution decisions).

	template <typename Struct_T>
	struct StructVectorWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		StructVectorWrapper(std::vector<Struct_T>& var, const wchar_t* rootTag, const wchar_t* elemTag, InfoClassTypes eOwnerClass)
			: WrappedVar(static_cast<void*>(&var), rootTag)
			, m_elemTag(elemTag)
			, m_eOwnerClass(eOwnerClass)
		{}

		std::vector<Struct_T>& ref() const { return *static_cast<std::vector<Struct_T>*>(m_ptr); }

		void checkSum(uint32_t& iSum) const
		{
			for (uint32_t i = 0, n = ref().size(); i < n; i++)
			{
				CvInfoUtil util(m_eOwnerClass, &ref()[i]);
				util.checkSum(iSum);
			}
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			if (pXML->TryMoveToXmlFirstChild(m_tag.c_str()))
			{
				const int iNum = pXML->GetXmlChildrenNumber(m_elemTag.c_str());
				ref().clear();
				if (iNum > 0 && pXML->TryMoveToXmlFirstChild())
				{
					if (pXML->TryMoveToXmlFirstOfSiblings(m_elemTag.c_str()))
					{
						do
						{
							ref().push_back(Struct_T());
							CvInfoUtil util(m_eOwnerClass, &ref().back());
							util.initDataMembers();
							util.readXml(pXML);
						} while (pXML->TryMoveToXmlNextSibling(m_elemTag.c_str()));
					}
					pXML->MoveToXmlParent();
				}
				pXML->MoveToXmlParent();
			}
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			CvXMLLoadUtility::CopyNonDefaultsFromVector(ref(), static_cast<const StructVectorWrapper*>(source)->ref());
		}

	private:
		const std::wstring m_elemTag;
		const InfoClassTypes m_eOwnerClass;
	};

	template <typename Struct_T>
	CvInfoUtil& addStruct(std::vector<Struct_T>& vec, const wchar_t* rootTag, const wchar_t* elemTag)
	{
		m_wrappedVars.push_back(new StructVectorWrapper<Struct_T>(vec, rootTag, elemTag, m_eInfoClass));
		return *this;
	}

	///==========================
	/// PropertyManipulators wrapper
	///==========================
	/// A self-contained sub-object that already knows how to read/checksum/merge itself.

	struct PropertyManipulatorsWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		PropertyManipulatorsWrapper(CvPropertyManipulators& var)
			: WrappedVar(static_cast<void*>(&var), L"")
		{}

		CvPropertyManipulators& ref() const { return *static_cast<CvPropertyManipulators*>(m_ptr); }

		void checkSum(uint32_t& iSum) const { ref().getCheckSum(iSum); }
		void readXml(CvXMLLoadUtility* pXML)  { ref().read(pXML); }
		void copyNonDefaults(const WrappedVar* source)
		{
			ref().copyNonDefaults(&static_cast<const PropertyManipulatorsWrapper*>(source)->ref());
		}
	};

	CvInfoUtil& add(CvPropertyManipulators& var)
	{
		m_wrappedVars.push_back(new PropertyManipulatorsWrapper(var));
		return *this;
	}

	///================
	/// BoolExpr wrapper
	///================
	/// Owns the heap-allocated expression: reads it under <tag>, deletes it on uninit (so the info's
	/// dtor must NOT also SAFE_DELETE it). copyNonDefaults STEALS ownership from the source (matching
	/// the legacy modular-merge transfer-and-null behaviour).

	struct BoolExprWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

	protected:
		BoolExprWrapper(const BoolExpr*& var, const wchar_t* tag)
			: WrappedVar(static_cast<void*>(&var), tag)
		{}

		const BoolExpr*& ref() const { return *static_cast<const BoolExpr**>(m_ptr); }

		void initVar()   { ref() = NULL; }
		void uninitVar() { SAFE_DELETE(ref()); }

		void checkSum(uint32_t& iSum) const
		{
			if (ref() != NULL)
				ref()->getCheckSum(iSum);
		}

		void readXml(CvXMLLoadUtility* pXML)
		{
			if (pXML->TryMoveToXmlFirstChild(m_tag.c_str()))
			{
				ref() = BoolExpr::read(pXML);
				pXML->MoveToXmlParent();
			}
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			if (ref() == NULL)
			{
				BoolExprWrapper* src = const_cast<BoolExprWrapper*>(static_cast<const BoolExprWrapper*>(source));
				ref() = src->ref();
				src->ref() = NULL;
			}
		}
	};

	CvInfoUtil& addBoolExpr(const BoolExpr*& var, const wchar_t* tag)
	{
		m_wrappedVars.push_back(new BoolExprWrapper(var, tag));
		return *this;
	}

	///==================================
	/// Fixed int-array wrapper (yields / commerce)
	///==================================
	/// A heap `int[size]` filled by a CvXMLLoadUtility reader (SetYields / SetCommerce<int>) from
	/// under <tag>; absent tag => array freed. The member stays `int*` (getters unchanged).

	struct FixedIntArrayWrapper : WrappedVar
	{
		friend struct CvInfoUtil;

		typedef int (CvXMLLoadUtility::*Reader)(int**);

	protected:
		FixedIntArrayWrapper(int*& var, const wchar_t* tag, Reader reader, int size)
			: WrappedVar(static_cast<void*>(&var), tag)
			, m_reader(reader)
			, m_size(size)
		{}

		int*& ref() const { return *static_cast<int**>(m_ptr); }

		void initVar()   { ref() = NULL; }
		void uninitVar() { SAFE_DELETE_ARRAY(ref()); }

		void checkSum(uint32_t& iSum) const { CheckSumI(iSum, m_size, ref()); }

		void readXml(CvXMLLoadUtility* pXML)
		{
			if (pXML->TryMoveToXmlFirstChild(m_tag.c_str()))
			{
				(pXML->*m_reader)(&ref());
				pXML->MoveToXmlParent();
			}
			else
			{
				SAFE_DELETE_ARRAY(ref());
			}
		}

		void copyNonDefaults(const WrappedVar* source)
		{
			int* src = static_cast<const FixedIntArrayWrapper*>(source)->ref();
			if (src == NULL)
				return;
			const int iDefault = 0;
			for (int j = 0; j < m_size; j++)
			{
				if ((ref() == NULL || ref()[j] == iDefault) && src[j] != iDefault)
				{
					if (ref() == NULL)
						CvXMLLoadUtility::InitList(&ref(), m_size, iDefault);
					ref()[j] = src[j];
				}
			}
		}

	private:
		const Reader m_reader;
		const int    m_size;
	};

	CvInfoUtil& addYields(int*& var, const wchar_t* tag)
	{
		m_wrappedVars.push_back(new FixedIntArrayWrapper(var, tag, &CvXMLLoadUtility::SetYields, NUM_YIELD_TYPES));
		return *this;
	}

	CvInfoUtil& addCommerce(int*& var, const wchar_t* tag)
	{
		m_wrappedVars.push_back(new FixedIntArrayWrapper(var, tag, &CvXMLLoadUtility::SetCommerce<int>, NUM_COMMERCE_TYPES));
		return *this;
	}

private:
	///========================================================
	/// Wrapped pointers to the data members of an info object
	///========================================================

	const InfoClassTypes m_eInfoClass;
	const bool m_bForceImmediate;   // nested struct-element FKs always resolve immediately (see ctor)
	std::vector<WrappedVar*> m_wrappedVars;
};

#endif