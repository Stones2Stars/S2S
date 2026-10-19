#pragma once
#ifndef CV_CITY_BREAKDOWN_H
#define CV_CITY_BREAKDOWN_H

//
//	CvCityBreakdown -- one city number and the parts it is made of, filled by the engine function that
//	calculates the number, in the same pass that calculates it (docs/reference/tooltip-look.md § THE CITY
//	NUMBERS). The shared city hover renders it and adds nothing of its own: no sums, no scale conversion, no
//	formula.
//

#include <map>
#include <vector>

class CvCity;
class CvInfo;

///<summary>Which city number a breakdown explains. The renderer takes its heading and its icon from this.</summary>
enum CityBreakdownSubject
{
	CITY_BREAKDOWN_FOOD,
	CITY_BREAKDOWN_PRODUCTION,
	CITY_BREAKDOWN_COMMERCE,
	CITY_BREAKDOWN_GOLD,
	CITY_BREAKDOWN_RESEARCH,
	CITY_BREAKDOWN_CULTURE,
	CITY_BREAKDOWN_ESPIONAGE,
	CITY_BREAKDOWN_MAINTENANCE,
	CITY_BREAKDOWN_HAPPINESS,
	CITY_BREAKDOWN_ANGER,
	CITY_BREAKDOWN_HEALTH,
	CITY_BREAKDOWN_UNHEALTH,
	CITY_BREAKDOWN_GREAT_PEOPLE,
	CITY_BREAKDOWN_DEFENSE,
	CITY_BREAKDOWN_TRADE_ROUTE,
	CITY_BREAKDOWN_EXPENSES,
	NUM_CITY_BREAKDOWN_SUBJECTS
};

///<summary>What a line does to the number: an amount added to it, a percent applied to what came before, the
/// subtotal the calculation reached at that point, the total it returned, or a note on why it is what it is.</summary>
enum CityBreakdownRole
{
	CITY_BREAKDOWN_AMOUNT,
	CITY_BREAKDOWN_PERCENT,
	CITY_BREAKDOWN_SUBTOTAL,
	CITY_BREAKDOWN_TOTAL,
	CITY_BREAKDOWN_NOTE
};

///<summary>The fixed name of a line that no source info names. CITY_BREAKDOWN_TERM_SOURCE marks a line about
/// one source info; CITY_BREAKDOWN_TERM_BUILDINGS through _PROCESSES name a run of sources of that kind
/// (CityBreakdownRun).</summary>
enum CityBreakdownTerm
{
	CITY_BREAKDOWN_TERM_SOURCE,
	CITY_BREAKDOWN_TERM_BUILDINGS,
	CITY_BREAKDOWN_TERM_HERITAGES,
	CITY_BREAKDOWN_TERM_SPECIALISTS,
	CITY_BREAKDOWN_TERM_CIVICS,
	CITY_BREAKDOWN_TERM_TECHS,
	CITY_BREAKDOWN_TERM_TRAITS,
	CITY_BREAKDOWN_TERM_PROJECTS,
	CITY_BREAKDOWN_TERM_BONUSES,
	CITY_BREAKDOWN_TERM_RELIGIONS,
	CITY_BREAKDOWN_TERM_CORPORATIONS,
	CITY_BREAKDOWN_TERM_PROCESSES,
	CITY_BREAKDOWN_TERM_OTHER,
	CITY_BREAKDOWN_TERM_SUBTOTAL,
	CITY_BREAKDOWN_TERM_TOTAL,
	CITY_BREAKDOWN_TERM_FRACTION_DROPPED,
	CITY_BREAKDOWN_TERM_DISORDER,
	CITY_BREAKDOWN_TERM_WORKED_TILES,
	CITY_BREAKDOWN_TERM_TRADE_ROUTES,
	CITY_BREAKDOWN_TERM_GOLDEN_AGE,
	CITY_BREAKDOWN_TERM_PRODUCED,
	CITY_BREAKDOWN_TERM_EATEN,
	CITY_BREAKDOWN_TERM_UNHEALTHY,
	CITY_BREAKDOWN_TERM_WASTED,
	CITY_BREAKDOWN_TERM_FOOD_PRODUCTION,
	CITY_BREAKDOWN_TERM_STARVATION_GUARD,
	CITY_BREAKDOWN_TERM_OVERFLOW,
	CITY_BREAKDOWN_TERM_CHOPPING,
	CITY_BREAKDOWN_TERM_FOOD_TO_PRODUCTION,
	CITY_BREAKDOWN_TERM_MINIMUM_PRODUCTION,
	CITY_BREAKDOWN_TERM_SLIDER_SHARE,
	CITY_BREAKDOWN_TERM_MAINTENANCE_NUM_CITIES,
	CITY_BREAKDOWN_TERM_MAINTENANCE_DISTANCE,
	CITY_BREAKDOWN_TERM_MAINTENANCE_COLONY,
	CITY_BREAKDOWN_TERM_MAINTENANCE_BUILDINGS,
	CITY_BREAKDOWN_TERM_CITY_PERCENTS,
	CITY_BREAKDOWN_TERM_WE_LOVE_THE_KING_DAY,
	// ---- wellbeing (CvCity::realizedWellbeing's raw-state terms) ----
	CITY_BREAKDOWN_TERM_SPECIALISTS_WELLBEING,
	CITY_BREAKDOWN_TERM_MILITARY_UNITS,
	CITY_BREAKDOWN_TERM_CELEBRITIES,
	CITY_BREAKDOWN_TERM_VASSALS,
	CITY_BREAKDOWN_TERM_REVOLUTION_SUCCESS,
	CITY_BREAKDOWN_TERM_EVENTS,
	CITY_BREAKDOWN_TERM_TEMPORARY_HAPPINESS,
	CITY_BREAKDOWN_TERM_OVERCROWDING,
	CITY_BREAKDOWN_TERM_NO_MILITARY,
	CITY_BREAKDOWN_TERM_FOREIGN_CULTURE,
	CITY_BREAKDOWN_TERM_RELIGION_STRIFE,
	CITY_BREAKDOWN_TERM_HURRY_ANGER,
	CITY_BREAKDOWN_TERM_CONSCRIPT_ANGER,
	CITY_BREAKDOWN_TERM_DEFIED_RESOLUTION,
	CITY_BREAKDOWN_TERM_WAR_WEARINESS,
	CITY_BREAKDOWN_TERM_REVOLUTION_REQUEST,
	CITY_BREAKDOWN_TERM_REVOLUTION_INDEX,
	CITY_BREAKDOWN_TERM_VASSAL_UNREST,
	CITY_BREAKDOWN_TERM_ESPIONAGE,
	CITY_BREAKDOWN_TERM_LANDMARKS,
	CITY_BREAKDOWN_TERM_POPULATION,
	CITY_BREAKDOWN_TERM_NO_UNHAPPINESS,
	CITY_BREAKDOWN_TERM_STATE_RELIGION,
	CITY_BREAKDOWN_TERM_TRADE_BASE_PROFIT,
	CITY_BREAKDOWN_TERM_TRADE_CITY_SIZE,
	CITY_BREAKDOWN_TERM_TRADE_CAPITAL_CONNECTION,
	CITY_BREAKDOWN_TERM_TRADE_OVERSEAS,
	CITY_BREAKDOWN_TERM_TRADE_FOREIGN,
	CITY_BREAKDOWN_TERM_TREASURY_UPKEEP,
	CITY_BREAKDOWN_TERM_CITY_MAINTENANCE,
	CITY_BREAKDOWN_TERM_CIVIC_UPKEEP,
	CITY_BREAKDOWN_TERM_UNIT_UPKEEP,
	CITY_BREAKDOWN_TERM_UNIT_SUPPLY,
	CITY_BREAKDOWN_TERM_CORPORATE_MAINTENANCE,
	CITY_BREAKDOWN_TERM_INFLATION,
	CITY_BREAKDOWN_TERM_FOREIGN_INCOME,
	CITY_BREAKDOWN_TERM_FOREIGN_PAYMENTS,
	NUM_CITY_BREAKDOWN_TERMS
};

///<summary>One line of a breakdown.</summary>
struct CityBreakdownLine
{
	CityBreakdownRole role;
	CityBreakdownTerm term;
	///<summary>The info a CITY_BREAKDOWN_TERM_SOURCE line is about; NULL for every other term.</summary>
	const CvInfo* source;
	///<summary>How many things the line counts -- worked tiles, citizens, specialists of one type; 0 when it
	/// names no count.</summary>
	int count;
	///<summary>Whether this amount line went through a percent stack of its own (a maintenance kind): then
	/// stackBase is the amount before the stack and stackPercent the stack.</summary>
	bool hasOwnStack;
	int64_t stackBase;
	int stackPercent;
	///<summary>The amount the line adds (CITY_BREAKDOWN_AMOUNT, SUBTOTAL, TOTAL), or the percent it applies
	/// (CITY_BREAKDOWN_PERCENT).</summary>
	int64_t value;
};

///<summary>Adjacent source lines of one role and one kind (buildings, heritages, specialists, civics...): where
/// they start, how many there are, the term naming the kind and what they add together. A display that cannot
/// fit every line shows a run as one line (docs/reference/tooltip-look.md § THE CITY NUMBERS).</summary>
struct CityBreakdownRun
{
	size_t firstLine;
	size_t lineCount;
	CityBreakdownRole role;
	CityBreakdownTerm term;
	int64_t total;
};

///<summary>A city number with its parts, in the order the calculation met them.</summary>
class CvCityBreakdown
{
public:
	///<summary>An empty breakdown of no subject; reset names the subject.</summary>
	CvCityBreakdown();

	///<summary>Forget every line and start over for eSubject.</summary>
	void reset(CityBreakdownSubject eSubject);

	///<summary>The city number this breakdown explains.</summary>
	CityBreakdownSubject getSubject() const { return m_eSubject; }
	///<summary>Every line, in the order the calculation added them.</summary>
	const std::vector<CityBreakdownLine>& getLines() const { return m_lines; }
	///<summary>The value of the TOTAL line, 0 before one was set.</summary>
	int64_t getTotal() const;
	///<summary>The value of the line under eTerm, 0 when there is none.</summary>
	int64_t getTermValue(CityBreakdownTerm eTerm) const;

	///<summary>An amount the calculation added, under a fixed name, with the count it stands for. A zero amount
	/// adds no line.</summary>
	void addAmount(CityBreakdownTerm eTerm, int64_t iValue, int iCount = 0);
	///<summary>An amount that went through its own percent stack before it was added: the base, the stack and
	/// what came out. A zero result adds no line.</summary>
	void addStackedAmount(CityBreakdownTerm eTerm, int64_t iBase, int iPercent, int64_t iValue);
	///<summary>An amount one source info delivered, with the count it stands for (specialists of one type). A
	/// zero amount adds no line.</summary>
	void addSourceAmount(const CvInfo* pSource, int64_t iValue, int iCount = 0);
	///<summary>A percent the calculation applied, under a fixed name. A zero percent adds no line.</summary>
	void addPercent(CityBreakdownTerm eTerm, int iPercent);
	///<summary>A percent one source info delivered. A zero percent adds no line.</summary>
	void addSourcePercent(const CvInfo* pSource, int iPercent);
	///<summary>The value the calculation reached before what follows, under eTerm ("Subtotal", "Produced").</summary>
	void addSubtotal(int64_t iValue, CityBreakdownTerm eTerm = CITY_BREAKDOWN_TERM_SUBTOTAL);
	///<summary>The number the calculation returned.</summary>
	void setTotal(int64_t iValue);
	///<summary>A line that states why the number is what it is, with no value (no maintenance while
	/// celebrating).</summary>
	void addNote(CityBreakdownTerm eTerm);

	///<summary>One line per source of a part the calculation summed -- amounts or percents by eRole --
	/// kind by kind (buildings, heritages, specialists, civics...), each largest first; whatever the named sources
	/// do not account for of iTotal goes
	/// on an "other" line, so the lines always add up to the part the calculation used. pCounts, when given, is
	/// how many things each source stands for (specialists of one type).</summary>
	void addSourceLines(const std::map<const CvInfo*, int64_t>& kPerSource, CityBreakdownRole eRole, int64_t iTotal,
		const std::map<const CvInfo*, int>* pCounts = NULL);
	///<summary>The percent side of iChannel at this city, one line per source that applied a percent to it,
	/// summed over every scope it reached, with the "other" line making up iPercentSum.</summary>
	void addPercentSources(const CvCity& city, int iChannel, int iPercentSum);
	///<summary>Every run of two or more adjacent source lines of one role and one known kind, in line order.</summary>
	void getSourceRuns(std::vector<CityBreakdownRun>& kRuns) const;

private:
	///<summary>Appends one line; the public adders call this.</summary>
	void addLine(CityBreakdownRole eRole, CityBreakdownTerm eTerm, const CvInfo* pSource, int64_t iValue);

	CityBreakdownSubject m_eSubject;
	std::vector<CityBreakdownLine> m_lines;
};

#endif // CV_CITY_BREAKDOWN_H
