#pragma once

//  $Header:
//------------------------------------------------------------------------------------------------
//
//  FILE:    CvPropertyManipulators.h
//
//  PURPOSE: Stores manipulators of generic properties for Civ4 classes (sources, interactions, propagators)
//
//------------------------------------------------------------------------------------------------
#ifndef CV_PROPERTY_MANIPULATORS_H
#define CV_PROPERTY_MANIPULATORS_H

class CvPropertySource;
class CvPropertyInteraction;
class CvPropertyPropagator;
class BoolExpr;
class IntExpr;

class CvPropertyManipulators
{
public:
	~CvPropertyManipulators();

	int getNumSources() const;
	// Delete + empty everything. The JSON load bridges call this at the top of their (re-runnable) mapFrom walk
	// -- mapFrom is idempotent by contract (CvInfo.h: an accumulating container clears at the top of its parse),
	// and an append-only bridge duplicated every source on the aliased full-registry re-run.
	void clear();

	CvPropertySource* getSource(int index) const;
	const std::vector<CvPropertySource*>& getSources() const { return m_apSources; }
	const boost::python::list cyGetSources() const;
	int addSource(PropertySourceTypes eType);

	int getNumInteractions() const;
	//CvPropertyInteraction* getInteraction(int index) const;
	const std::vector<CvPropertyInteraction*>& getInteractions() const { return m_apInteractions; }
	int addInteraction(PropertyInteractionTypes eType);

	int getNumPropagators() const;
	//CvPropertyPropagator* getPropagator(int index) const;
	const std::vector<CvPropertyPropagator*>& getPropagators() const { return m_apPropagators; }
	int addPropagator(PropertyPropagatorTypes eType);

	void buildDisplayString(CvWStringBuffer& szBuffer) const;

	bool read(CvXMLLoadUtility* pXML, const wchar_t* szTagName = L"PropertyManipulators");
	void copyNonDefaults(const CvPropertyManipulators* pProp);

	// --- programmatic construction (the JSON->manipulator load bridge, property-audit.md increment A). Build a
	// fully-configured source/propagator directly (mirroring what read() builds from XML) and append it. eObject =
	// which GameObject scope the source is active on (NO_GAMEOBJECT = any). pActive = the translated `enabled`
	// gate (increment 4; ownership passes to the source/propagator). pAmount overload = an expression amount
	// (the per-POPULATION each>1 tree); ownership passes to the source. ---
	void addConstantSource(PropertyTypes eProp, int iAmount, GameObjectTypes eObject = NO_GAMEOBJECT,
		RelationTypes eRelation = NO_RELATION, int iRelationData = 0, const BoolExpr* pActive = NULL);
	void addConstantSource(PropertyTypes eProp, const IntExpr* pAmount, GameObjectTypes eObject,
		RelationTypes eRelation = NO_RELATION, int iRelationData = 0, const BoolExpr* pActive = NULL);
	void addDecaySource(PropertyTypes eProp, int iPercent, int iNoDecayAmount, GameObjectTypes eObject);
	void addAttributeConstantSource(PropertyTypes eProp, AttributeTypes eAttribute, int iAmount, GameObjectTypes eObject);
	void addDiffusePropagator(PropertyTypes eProp, int iPercent, GameObjectTypes eObject,
		GameObjectTypes eTargetObject, RelationTypes eTargetRelation, int iTargetDistance,
		const BoolExpr* pActive = NULL);

	void getCheckSum(uint32_t& iSum) const;

protected:
	std::vector<CvPropertySource*> m_apSources;
	std::vector<CvPropertyInteraction*> m_apInteractions;
	std::vector<CvPropertyPropagator*> m_apPropagators;
};

#endif