#include "parameterset.h"
#include "comment.h"
#include "modcontext.h"
#include "expression.h"
#include "printutils.h"
#include <boost/property_tree/json_parser.hpp>

std::string ParameterSet::parameterSetsKey("parameterSets");
std::string ParameterSet::fileFormatVersionKey("fileFormatVersion");
std::string ParameterSet::fileFormatVersionValue("1");

boost::optional<pt::ptree &> ParameterSet::parameterSets()
{
	return root.get_child_optional(ParameterSet::parameterSetsKey);
}

std::vector<std::string> ParameterSet::getParameterNames()
{
	std::vector<std::string> names;

	boost::optional<pt::ptree &> sets = parameterSets();
	if (sets.is_initialized()) {
		for (const auto &v : sets.get()) {
			names.push_back(v.first);
		}
	}
	return names;
}

bool ParameterSet::setNameExists(const std::string &setName){
	boost::optional<pt::ptree &> sets = parameterSets();
	if (sets.is_initialized()) {
		for (const auto &v : sets.get()) {
			if(v.first == setName) return true;
		}
	}
	return false;
}

boost::optional<pt::ptree &> ParameterSet::getParameterSet(const std::string &setName)
{
	boost::optional<pt::ptree &> sets = parameterSets();
	if (!sets.is_initialized()) {
		return sets;
	}

	pt::ptree::assoc_iterator set = sets.get().find(pt::ptree::key_type(setName));
	if(set!=sets.get().not_found()) {
		return set->second;
	}
	return sets;
}

void ParameterSet::addParameterSet(const std::string setName, const pt::ptree & set)
{
	boost::optional<pt::ptree &> sets = parameterSets();
	if (sets.is_initialized()) {
		sets.get().erase(pt::ptree::key_type(setName));
		sets.get().push_back(pt::ptree::value_type(setName,set));
	}
	else {
		pt::ptree child;
		child.push_back(pt::ptree::value_type(setName,set));
		root.push_back(pt::ptree::value_type(ParameterSet::parameterSetsKey,child));
	}
}

/*!
	Returns true if the file was succesfully read
*/
bool ParameterSet::readParameterSet(const std::string &filename)
{
	try {
		pt::read_json(filename, this->root);
		return true;
	}
	catch (const pt::json_parser_error &e) {
		PRINTB("ERROR: Cannot open Parameter Set '%s': %s", filename % e.what());
	}
	return false;
}

void ParameterSet::writeParameterSet(const std::string &filename)
{
	// Always force default version for now
	root.put<std::string>(fileFormatVersionKey, fileFormatVersionValue);
	try {
		pt::write_json(filename, this->root);
	}
	catch (const pt::json_parser_error &e) {
		PRINTB("ERROR: Cannot write Parameter Set '%s': %s", filename % e.what());
	}
}

void ParameterSet::applyParameterSet(FileModule *fileModule, const std::string &setName)
{
	if (fileModule == nullptr || this->root.empty()) return;
	try {
		ModuleContext ctx;
		boost::optional<pt::ptree &> set = getParameterSet(setName);
		for (auto &assignment : fileModule->scope.assignments) {
			for (auto &v : set.get()) {
				if (v.first == assignment.name) {
					const ValuePtr defaultValue = assignment.expr->evaluate(&ctx);
					if (defaultValue->type() == Value::ValueType::STRING) {
						assignment.expr = shared_ptr<Expression>(new Literal(ValuePtr(v.second.data())));
					}
					else if (defaultValue->type() == Value::ValueType::BOOL) {
						assignment.expr = shared_ptr<Expression>(new Literal(ValuePtr(v.second.get_value<bool>())));
					} else {
						shared_ptr<Expression> params = CommentParser::parser(v.second.data().c_str());
						if (!params) continue;
						ModuleContext ctx;
						if (defaultValue->type() == params->evaluate(&ctx)->type()) {
							assignment.expr = params;
						}
					}
				}
			}
		}
	}
	catch (std::exception const& e) {
		PRINTB("ERROR: Cannot apply parameter Set: %s", e.what());
	}
}

