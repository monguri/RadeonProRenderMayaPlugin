#include "FireRenderNormal.h"

#include <maya/MFnTypedAttribute.h>
#include "maya/MGlobal.h"

namespace
{
	namespace Attribute
	{
		MObject color;
		MObject source;		// will be replaced with "color" when upgrading scene
		MObject strength;
		MObject	output;
	}
}


void* FireMaya::Normal::creator()
{
	return new FireMaya::Normal;
}

void FireMaya::Normal::OnFileLoaded()
{
	// Older version of this node used texture file name. Convert it to regular graph node.
	ConvertFilenameToFileInput(Attribute::source, Attribute::color);
}

MStatus FireMaya::Normal::initialize()
{
#define DEPRECATED_PARAM(attr) \
	CHECK_MSTATUS(attr.setCached(false)); \
	CHECK_MSTATUS(attr.setStorable(false)); \
	CHECK_MSTATUS(attr.setHidden(true));

	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;

	Attribute::color = nAttr.createColor("color", "c");
	MAKE_INPUT(nAttr);

	Attribute::source = tAttr.create("filename", "src", MFnData::kString);
	tAttr.setUsedAsFilename(true);
	MAKE_INPUT_CONST(tAttr);
	DEPRECATED_PARAM(tAttr);

	Attribute::strength = nAttr.create("strength", "str", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMax(1.0);
	nAttr.setSoftMin(0.0);

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::source));
	CHECK_MSTATUS(addAttribute(Attribute::strength));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::source, Attribute::output)); //
	CHECK_MSTATUS(attributeAffects(Attribute::strength, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Normal::GetValue(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::Value color = scope.GetConnectedValue(shaderNode.findPlug(Attribute::color));
	if (color)
	{
		frw::NormalMapNode imageNode(scope.MaterialSystem());
		imageNode.SetMap(color);

		frw::Value strength = scope.GetValue(shaderNode.findPlug(Attribute::strength));
		if (strength != 1.)
			imageNode.SetValue("bumpscale", strength);

		return imageNode;
	}

	return nullptr;
}