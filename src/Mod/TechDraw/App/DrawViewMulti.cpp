/***************************************************************************
 *   Copyright (c) WandererFan            (wandererfan@gmail.com) 2016     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <sstream>

#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
# include <BRep_Builder.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pnt.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>

#endif

#include <chrono>

# include <QFile>
# include <QFileInfo>

#include <App/Application.h>
#include <App/Material.h>
#include <Base/BoundBox.h>
#include <Base/Exception.h>
#include <Base/Console.h>
#include <Base/Parameter.h>

#include <Mod/Part/App/PartFeature.h>

#include "Geometry.h"
#include "GeometryObject.h"
#include "DrawViewMulti.h"

using namespace TechDraw;
using namespace std;


//===========================================================================
// DrawViewMulti
//===========================================================================

PROPERTY_SOURCE(TechDraw::DrawViewMulti, TechDraw::DrawViewPart)

DrawViewMulti::DrawViewMulti()
{
    static const char *group = "Projection";

    //properties that affect Geometry
    ADD_PROPERTY_TYPE(Sources ,(0),group,App::Prop_None,"3D Shapes to view");

    //Source is replaced by Sources in Multi
    Source.setStatus(App::Property::ReadOnly,true);

    geometryObject = nullptr;
}

DrawViewMulti::~DrawViewMulti()
{
}

short DrawViewMulti::mustExecute() const
{
    short result = 0;
    if (!isRestoring()) {
        result  = (Sources.isTouched());
    }
    if (result) {
        return result;
    }
    return TechDraw::DrawViewPart::mustExecute();
}

void DrawViewMulti::onChanged(const App::Property* prop)
{
    if (!isRestoring()) {
        //Base::Console().Message("TRACE - DVM::onChanged(%s) - %s\n",prop->getName(),Label.getValue());
        if (prop == &Sources) {
            const std::vector<App::DocumentObject*>& links = Sources.getValues();
            if (!links.empty()) {
                Source.setValue(links.front());
            }
        }
    }

    DrawViewPart::onChanged(prop);
}

App::DocumentObjectExecReturn *DrawViewMulti::execute(void)
{
    const std::vector<App::DocumentObject*>& links = Sources.getValues();
    if (links.empty())  {
        Base::Console().Log("INFO - DVM::execute - No Sources - creation?\n");
        return DrawViewPart::execute();
    }

    //Base::Console().Message("TRACE - DVM::execute() - %s/%s\n",getNameInDocument(),Label.getValue());

    (void) DrawView::execute();          //make sure Scale is up to date

    BRep_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);
    for (auto& l:links) {
        const Part::TopoShape &partTopo = static_cast<Part::Feature*>(l)->Shape.getShape();
        BRepBuilderAPI_Copy BuilderCopy(partTopo.getShape());
        TopoDS_Shape shape = BuilderCopy.Shape();
        builder.Add(comp, shape);
    }
    m_compound = comp;

    gp_Pnt inputCenter;
    try {
        inputCenter = TechDrawGeometry::findCentroid(comp,
                                                     Direction.getValue());
        TopoDS_Shape mirroredShape = TechDrawGeometry::mirrorShape(comp,
                                                    inputCenter,
                                                    Scale.getValue());
        gp_Ax2 viewAxis = getViewAxis(Base::Vector3d(inputCenter.X(),inputCenter.Y(),inputCenter.Z()),Direction.getValue());
        geometryObject = buildGeometryObject(mirroredShape,viewAxis);

#if MOD_TECHDRAW_HANDLE_FACES
        extractFaces();
#endif //#if MOD_TECHDRAW_HANDLE_FACES
    }
    catch (Standard_Failure) {
        Handle_Standard_Failure e1 = Standard_Failure::Caught();
        Base::Console().Log("LOG - DVM::execute - projection failed for %s - %s **\n",getNameInDocument(),e1->GetMessageString());
        return new App::DocumentObjectExecReturn(e1->GetMessageString());
    }

    return App::DocumentObject::StdReturn;
}

// Python Drawing feature ---------------------------------------------------------

namespace App {
/// @cond DOXERR
PROPERTY_SOURCE_TEMPLATE(TechDraw::DrawViewMultiPython, TechDraw::DrawViewMulti)
template<> const char* TechDraw::DrawViewMultiPython::getViewProviderName(void) const {
    return "TechDrawGui::ViewProviderViewProviderViewPart";
}
/// @endcond

// explicit template instantiation
template class TechDrawExport FeaturePythonT<TechDraw::DrawViewMulti>;
}
