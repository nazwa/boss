/*	Better Oblivion Sorting Software
	
	Quick and Dirty Load Order Utility
	(Making C++ look like the scripting language it isn't.)

    Copyright (C) 2009-2010  Random/Random007/jpearce & the BOSS development team
    http://creativecommons.org/licenses/by-nc-nd/3.0/

	$Revision: 2188 $, $Date: 2011-01-20 10:05:16 +0000 (Thu, 20 Jan 2011) $
*/

#ifndef __BOSS_MODLIST_GRAM_H__
#define __BOSS_MODLIST_GRAM_H__

#ifndef BOOST_SPIRIT_UNICODE
#define BOOST_SPIRIT_UNICODE 
#endif

#include "Parsing/Data.h"
#include "Parsing/Skipper.h"
#include "Common/Globals.h"
#include "Support/Helpers.h"
#include <sstream>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/home/phoenix/object/construct.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/unordered_set.hpp>

namespace boss {
	namespace unicode = boost::spirit::unicode;
	namespace phoenix = boost::phoenix;
	namespace qi = boost::spirit::qi;

	using namespace std;
	using namespace qi::labels;

	using qi::skip;
	using qi::eol;
	using qi::lexeme;
	using qi::on_error;
	using qi::fail;
	using qi::lit;
	using qi::omit;
	using qi::eps;
	using qi::int_;

	using unicode::char_;
	using unicode::upper;
	using unicode::digit;

	///////////////////////////////
	//Modlist/Masterlist Grammar
	///////////////////////////////

	boost::unordered_set<string> setVars;  //Vars set by masterlist.
	boost::unordered_set<string>::iterator pos;
	bool storeItem = true, storeMessage = true;
	vector<string> openGroups;  //Need to keep track of which groups are open to match up endings properly in old format.

	//Checks if a masterlist variable is defined.
	void CheckVar(bool& result, string var) {
		if (setVars.find(var) == setVars.end())
			result = false;
		else
			result = true;
		return;
	}

	//Checks if the given mod has a version for which the comparison holds true.
	void CheckVersion(bool& result, string var) {
		char comp = var[0];
		size_t pos = var.find("|") + 1;
		string version = var.substr(1,pos-2);
		string mod = var.substr(pos);
		result = false;

		if (Exists(data_path / mod)) {
			string trueVersion;
			if (IsGhosted(data_path / mod)) 
				trueVersion = GetModHeader(data_path / fs::path(mod + ".ghost"));
			else 
				trueVersion = GetModHeader(data_path / mod);

			switch (comp) {
			case '>':
				if (trueVersion.compare(version) > 0)
					result = true;
				break;
			case '<':
				if (trueVersion.compare(version) < 0)
					result = true;
				break;
			case '=':
				if (version == trueVersion)
					result = true;
				break;
			}
		}
		return;
	}

	//Checks if the given mod has the given checksum.
	void CheckSum(bool& result, int sum, string mod) {
		result = false;
		if (Exists(data_path / mod)) {
			int CRC;
			if (IsGhosted(data_path / mod)) 
				CRC = GetCrc32(data_path / fs::path(mod + ".ghost"));
			else
				CRC = GetCrc32(data_path / mod);

			if (sum == CRC)
				result = true;
		}
		return;
	}

	//Stores a message, should it be appropriate.
	void StoreMessage(vector<message>& messages, message currentMessage) {
		if (storeMessage)
			messages.push_back(currentMessage);
		return;
	}

	//Stores the given item, should it be appropriate, and records any changes to open groups.
	void StoreItem(vector<item>& list, item currentItem) {
		if (currentItem.type == BEGINGROUP) {
			openGroups.push_back(currentItem.name.string());
		} else if (currentItem.type == ENDGROUP) {
			openGroups.pop_back();
		}
		if (storeItem)
			list.push_back(currentItem);
		return;
	}

	//Defines the given masterlist variable, if appropriate.
	void StoreVar(bool result, string var) {
		if (result)
			setVars.insert(var);
		return;
	}

	//Evaluate a single conditional.
	void EvaluateConditional(bool& result, metaType type, bool condition) {
		result = false;
		if (type == IF && condition == true)
			result = true;
		else if (type == IFNOT && condition == false)
			result = true;
		return;
	}

	//Evaluate the second half of a complex conditional.
	void EvaluateCompoundConditional(bool& result, string andOr, bool condition) {
		if (andOr == "||" && condition == true)
			result = true;
		else if (andOr == "&&" && result == true && condition == false)
			result = false;
	}

	//MF1 compatibility function.
	//Possibly a way to unify this with the MF2 check?
	void EvalOldFCOMConditional(bool& result, char var) {
		result = false;
		pos = setVars.find("FCOM");
		if (var == '>' && pos != setVars.end())
				result = true;
		else if (var == '<' && pos == setVars.end())
				result = true;
		return;
	}

	//MF1 compatibility function.
	//Possibly a way to unify this with the MF2 check?
	void EvalMessKey(keyType key) {
		if (key == OOOSAY) {
			pos = setVars.find("OOO");
			if (pos == setVars.end())
				storeMessage = false;
		} else if (key == BCSAY) {
			pos = setVars.find("BC");
			if (pos == setVars.end())
				storeMessage = false;
		}
		return;
	}
	
	//Turns a given string into a path. Can't be done directly because of the openGroups checks.
	void path(fs::path& p, string const itemName) {
		if (itemName.length() == 0 && openGroups.size() > 0) 
			p = fs::path(openGroups.back());
		else
			p = fs::path(itemName);
		return;
	}

	//Old and new formats grammar.
	template <typename Iterator>
	struct modlist_grammar : qi::grammar<Iterator, vector<item>(), Skipper<Iterator> > {
		modlist_grammar() : modlist_grammar::base_type(modList, "modlist_grammar") {

			vector<message> noMessages;  //An empty set of messages.

			modList = (metaLine | listItem[phoenix::bind(&StoreItem, _val, _1)]) % +eol;

			metaLine =
				(conditionals
				>> (lit("SET")
				> ':'
				> charString))[phoenix::bind(&StoreVar, _1, _2)];

			listItem %= 
				omit[(oldConditional | (conditionals))[phoenix::ref(storeItem) = _1]]
				> ItemType
				> -lit(":")
				> itemName
				> itemMessages;

			ItemType %= 
				(groupKey >> -lit(":"))
				| eps[_val = MOD];

			itemName = 
				charString[phoenix::bind(&path, _val, _1)]
				| eps[phoenix::bind(&path, _val, "")];

			itemMessages = 
				(+eol
				>> itemMessage[phoenix::bind(&StoreMessage, _val, _1)] % +eol)
				| eps[_1 = noMessages];

			itemMessage %= 
				omit[(oldConditional | conditionals)[phoenix::ref(storeMessage) = _1]]
				>> messageKeyword[&EvalMessKey]
				> -lit(":")
				> messageString;

			charString %= lexeme[skip(lit("//") >> *(char_ - eol))[+(char_ - eol)]]; //String, but skip comment if present.

			messageString %= lexeme[+(char_ - eol)]; //String, with no skipper. Used for messages, which can contain web links which the skipper would cut out.

			messageKeyword %= masterlistMsgKey;

			oldConditional = (char_(">") |  char_("<"))		[phoenix::bind(&EvalOldFCOMConditional, _val, _1)];

			conditionals = 
				(conditional[_val = _1] 
				> *((andOr > conditional)			[phoenix::bind(&EvaluateCompoundConditional, _val, _1, _2)]))
				| eps[_val = true];

			andOr %= unicode::string("&&") | unicode::string("||");

			conditional = (metaKey > '(' > condition > ')')	[phoenix::bind(&EvaluateConditional, _val, _1, _2)];

			condition = 
				variable[phoenix::bind(&CheckVar, _val, _1)]
				| version[phoenix::bind(&CheckVersion, _val, _1)]
				| (int_ > '|' > mod)[phoenix::bind(&CheckSum, _val, _1, _2)] //A CRC-32 checksum, as calculated by BOSS, followed by the mod it applies to.
				| mod[_val = phoenix::bind(&Exists, data_path / _1)];  //Checks if the given mod exists directly.

			variable %= '$' > +(char_ - ')');  //A masterlist variable, prepended by a '$' character to differentiate between vars and mods.

			mod %= lexeme['\"' > +(char_ - '\"') > '\"'];  //A mod, enclosed in quotes.

			version %=   //A version, followed by the mod it applies to.
				(char_('=') | char_('>') | char_('<'))
				> lexeme[+(char_ - '|')]
				> char_('|')
				> mod;

			modList.name("modList");
			metaLine.name("metaLine");
			listItem.name("listItem");
			ItemType.name("ItemType");
			itemName.name("itemName");
			itemMessages.name("itemMessages");
			itemMessage.name("itemMessage");
			charString.name("charString");
			messageString.name("messageString");
			upperString.name("upperString");
			messageKeyword.name("messageKeyword");
			oldConditional.name("oldConditional");
			conditionals.name("conditional");
			andOr.name("andOr");
			conditional.name("conditional");
			condition.name("condition");
			variable.name("variable");
			mod.name("mod");
			version.name("version");
			
			on_error<fail>(modList,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(listItem,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(ItemType,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(itemName,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(itemMessages,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(itemMessage,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(charString,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(messageString,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(messageKeyword,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));

			on_error<fail>(metaLine,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(upperString,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(oldConditional,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(conditionals,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(andOr,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(conditional,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(condition,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(variable,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(mod,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));
			on_error<fail>(version,phoenix::bind(&modlist_grammar<Iterator>::SyntaxErr, this, _1, _2, _3, _4));

		}

		typedef Skipper<Iterator> skipper;

		qi::rule<Iterator, vector<item>(), skipper> modList;
		qi::rule<Iterator, item(), skipper> listItem;
		qi::rule<Iterator, itemType(), skipper> ItemType;
		qi::rule<Iterator, fs::path(), skipper> itemName;
		qi::rule<Iterator, vector<message>(), skipper> itemMessages;
		qi::rule<Iterator, message(), skipper> itemMessage;
		qi::rule<Iterator, string(), skipper> charString, upperString, messageString, variable, mod, version, andOr;
		qi::rule<Iterator, keyType(), skipper> messageKeyword;
		qi::rule<Iterator, bool(), skipper> conditional, conditionals, condition, oldConditional;
		qi::rule<Iterator, skipper> metaLine;
		
		void SyntaxErr(Iterator const& first, Iterator const& last, Iterator const& errorpos, boost::spirit::info const& what) {
			cout << "Masterlist parsing error: " << what;
		}
	};
}
#endif