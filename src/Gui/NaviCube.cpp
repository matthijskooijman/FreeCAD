/***************************************************************************
 *   Copyright (c) 2017 Kustaa Nyholm  <kustaa.nyholm@sparetimelabs.com>   *
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
# include <float.h>
# ifdef FC_OS_WIN32
#  include <windows.h>
# endif
# ifdef FC_OS_MACOSX
# include <OpenGL/gl.h>
# else
# include <GL/gl.h>
# endif
# include <Inventor/SbBox.h>
# include <Inventor/actions/SoGetBoundingBoxAction.h>
# include <Inventor/actions/SoGetMatrixAction.h>
# include <Inventor/actions/SoHandleEventAction.h>
# include <Inventor/actions/SoToVRML2Action.h>
# include <Inventor/actions/SoWriteAction.h>
# include <Inventor/elements/SoViewportRegionElement.h>
# include <Inventor/manips/SoClipPlaneManip.h>
# include <Inventor/nodes/SoBaseColor.h>
# include <Inventor/nodes/SoCallback.h>
# include <Inventor/nodes/SoCoordinate3.h>
# include <Inventor/nodes/SoCube.h>
# include <Inventor/nodes/SoDirectionalLight.h>
# include <Inventor/nodes/SoEventCallback.h>
# include <Inventor/nodes/SoFaceSet.h>
# include <Inventor/nodes/SoImage.h>
# include <Inventor/nodes/SoIndexedFaceSet.h>
# include <Inventor/nodes/SoLightModel.h>
# include <Inventor/nodes/SoLocateHighlight.h>
# include <Inventor/nodes/SoMaterial.h>
# include <Inventor/nodes/SoMaterialBinding.h>
# include <Inventor/nodes/SoOrthographicCamera.h>
# include <Inventor/nodes/SoPerspectiveCamera.h>
# include <Inventor/nodes/SoRotationXYZ.h>
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoShapeHints.h>
# include <Inventor/nodes/SoSwitch.h>
# include <Inventor/nodes/SoTransform.h>
# include <Inventor/nodes/SoTranslation.h>
# include <Inventor/nodes/SoSelection.h>
# include <Inventor/nodes/SoText2.h>
# include <Inventor/actions/SoBoxHighlightRenderAction.h>
# include <Inventor/events/SoEvent.h>
# include <Inventor/events/SoKeyboardEvent.h>
# include <Inventor/events/SoLocation2Event.h>
# include <Inventor/events/SoMotion3Event.h>
# include <Inventor/events/SoMouseButtonEvent.h>
# include <Inventor/actions/SoRayPickAction.h>
# include <Inventor/projectors/SbSphereSheetProjector.h>
# include <Inventor/SoOffscreenRenderer.h>
# include <Inventor/SoPickedPoint.h>
# include <Inventor/VRMLnodes/SoVRMLGroup.h>
# include <QEventLoop>
# include <QKeyEvent>
# include <QWheelEvent>
# include <QMessageBox>
# include <QTimer>
# include <QStatusBar>
# include <QBitmap>
# include <QMimeData>
#endif

#include <sstream>
#include <Base/Console.h>
#include <Base/Stream.h>
#include <Base/FileInfo.h>
#include <Base/Rotation.h>
#include <Base/Sequencer.h>
#include <Base/Tools.h>
#include <Base/UnitsApi.h>

#include "View3DInventorViewer.h"
#include "View3DInventor.h"
#include "Application.h"
#include "MainWindow.h"
#include "MDIView.h"
#include "Command.h"

#include "NaviCube.h"

#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QImage>
#include <QPainterPath>
#include <QApplication>

#if defined(HAVE_QT5_OPENGL)
# include <QOpenGLTexture>
#endif

//#include <OpenGL/glu.h>
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>


using namespace Eigen;
using namespace std;
using namespace Gui;

// TODO
// ortho / persp
// stay in window
// corner angle
// menu actions
// size
// XYZ position
// menu feedback
// colors
// antialise cube icon
// translation
// DONE
// - permanent menu ("NaviCube_Menu"
// - improved hit testing
// - improved graphics (text now black)
// - first stab at supporting translations

class Face {
public:
	int m_FirstVertex;
	int m_VertexCount;
	GLuint m_TextureId;
	QColor m_Color;
	int m_PickId;
	GLuint m_PickTextureId;
	int m_RenderPass;
	Face(
		 int firstVertex,
		 int vertexCount,
		 GLuint textureId,
		 int pickId,
		 GLuint pickTextureId,
		 const QColor& color,
		 int  renderPass
		)
	{
		m_FirstVertex = firstVertex;
		m_VertexCount = vertexCount;
		m_TextureId = textureId;
		m_PickId = pickId;
		m_PickTextureId = pickTextureId;
		m_Color = color;
		m_RenderPass = renderPass;
	}
};

class NaviCubeImplementation : public ParameterGrp::ObserverType {
public:
	NaviCubeImplementation(Gui::View3DInventorViewer*);
	virtual ~ NaviCubeImplementation();
	void drawNaviCube();
	void createContextMenu(const std::vector<std::string>& cmd);

	/// Observer message from the ParameterGrp
	virtual void OnChange(ParameterGrp::SubjectType &rCaller,ParameterGrp::MessageType Reason);

	bool processSoEvent(const SoEvent* ev);
private:
	bool mousePressed(short x, short y);
	bool mouseReleased(short x, short y);
	bool mouseMoved(short x, short y);
	int pickFace(short x, short y);
	bool inDragZone(short x, short y);

	void handleResize();
	void handleMenu();

	void setHilite(int);

	void initNaviCube(QtGLWidget*);
	void addFace(const Vector3f&, const Vector3f&, int, int, int, int,bool flag=false);

	GLuint createCubeFaceTex(QtGLWidget*, float, float, const char*);
	GLuint createButtonTex(QtGLWidget*, int);
	GLuint createMenuTex(QtGLWidget*, bool);

	void setView(float ,float );
	void rotateView(int ,float );

	QString str(char* str);
	char* enum2str(int);
	QMenu* createNaviCubeMenu();
public:
	enum { //
		TEX_FRONT = 1, // 0 is reserved for 'nothing picked'
		TEX_REAR,
		TEX_TOP,
		TEX_BOTTOM,
		TEX_LEFT,
		TEX_RIGHT,
		TEX_BACK_FACE,
		TEX_FRONT_FACE,
		TEX_CORNER_FACE,
		TEX_BOTTOM_RIGHT_REAR,
		TEX_BOTTOM_FRONT_RIGHT,
		TEX_BOTTOM_LEFT_FRONT,
		TEX_BOTTOM_REAR_LEFT,
		TEX_TOP_RIGHT_FRONT,
		TEX_TOP_FRONT_LEFT,
		TEX_TOP_LEFT_REAR,
		TEX_TOP_REAR_RIGHT,
		TEX_ARROW_NORTH,
		TEX_ARROW_SOUTH,
		TEX_ARROW_EAST,
		TEX_ARROW_WEST,
		TEX_ARROW_RIGHT,
		TEX_ARROW_LEFT,
		TEX_VIEW_MENU_ICON,
		TEX_VIEW_MENU_FACE
	};
	enum {
		DIR_UP,DIR_RIGHT,DIR_OUT
	};
	Gui::View3DInventorViewer* m_View3DInventorViewer;
	void drawNaviCube(bool picking);

	int m_OverSample = 4;
	int m_CubeWidgetSize = 0;
	int m_CubeWidgetPosX = 0;
	int m_CubeWidgetPosY = 0;
	int m_PrevWidth = 0;
	int m_PrevHeight = 0;
	QColor m_TextColor;
	QColor m_HiliteColor;
	QColor m_ButtonColor;
	QColor m_FrontFaceColor;
	QColor m_BackFaceColor;
	int m_HiliteId = 0;
	bool m_MouseDown = false;
	bool m_Dragging = false;
	bool m_MightDrag = false;
    NaviCube::Corner m_Corner = NaviCube::TopRightCorner;

	QtGLFramebufferObject* m_PickingFramebuffer;

	bool m_NaviCubeInitialised = false;

	vector<GLubyte> m_IndexArray;
	vector<Vector2f> m_TextureCoordArray;
	vector<Vector3f> m_VertexArray;
	map<int,GLuint> m_Textures;
	vector<Face*> m_Faces;
	vector<int> m_Buttons;
#if defined(HAVE_QT5_OPENGL)
	vector<QOpenGLTexture *> m_glTextures;
#endif
	static vector<string> m_commands;
	static vector<string> m_labels;
	QMenu* m_Menu;
};

NaviCube::NaviCube(Gui::View3DInventorViewer* viewer) {
	m_NaviCubeImplementation = new NaviCubeImplementation(viewer);
}

NaviCube::~NaviCube() {
	delete m_NaviCubeImplementation;
}

void NaviCube::drawNaviCube() {
	m_NaviCubeImplementation->drawNaviCube();
}

void NaviCube::createContextMenu(const std::vector<std::string>& cmd) {
	m_NaviCubeImplementation->createContextMenu(cmd);
}

bool NaviCube::processSoEvent(const SoEvent* ev) {
	return m_NaviCubeImplementation->processSoEvent(ev);
}


vector<string> NaviCubeImplementation::m_commands;
vector<string> NaviCubeImplementation::m_labels;

void NaviCube::setCorner(Corner c) {
	m_NaviCubeImplementation->m_Corner = c;
	m_NaviCubeImplementation->m_PrevWidth = 0;
	m_NaviCubeImplementation->m_PrevHeight = 0;
}

NaviCubeImplementation::NaviCubeImplementation(
	Gui::View3DInventorViewer* viewer) {

	m_View3DInventorViewer = viewer;

	auto hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/NaviCube");
	hGrp->Attach(this);

	OnChange(*hGrp, "TextColor");
	OnChange(*hGrp, "FrontColor");
	OnChange(*hGrp, "BackColor");
	OnChange(*hGrp, "HiliteColor");
	OnChange(*hGrp, "ButtonColor");
	OnChange(*hGrp, "CubeSize");

	m_PickingFramebuffer = NULL;
	m_Menu = createNaviCubeMenu();
}

NaviCubeImplementation::~NaviCubeImplementation() {
	auto hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/NaviCube");
	hGrp->Detach(this);

	delete m_Menu;
	if (m_PickingFramebuffer)
		delete m_PickingFramebuffer;
	for (vector<Face*>::iterator f = m_Faces.begin(); f != m_Faces.end(); f++)
		delete *f;
#if defined(HAVE_QT5_OPENGL)
	for (vector<QOpenGLTexture *>::iterator t = m_glTextures.begin(); t != m_glTextures.end(); t++)
		delete *t;
#endif
}

void NaviCubeImplementation::OnChange(ParameterGrp::SubjectType &rCaller, ParameterGrp::MessageType reason)
{
	const auto & rGrp = static_cast<ParameterGrp &>(rCaller);

	if (strcmp(reason,"TextColor") == 0) {
		m_TextColor.setRgba(rGrp.GetUnsigned(reason, QColor(0,0,0,255).rgba()));
	} else if (strcmp(reason,"FrontColor") == 0) {
		m_FrontFaceColor.setRgba(rGrp.GetUnsigned(reason, QColor(255,255,255,128).rgba()));
	} else if (strcmp(reason,"BackColor") == 0) {
		m_BackFaceColor.setRgba(rGrp.GetUnsigned(reason, QColor(226,233,239,128).rgba()));
	} else if (strcmp(reason,"HiliteColor") == 0) {
		m_HiliteColor.setRgba(rGrp.GetUnsigned(reason, QColor(170,226,255).rgba()));
	} else if (strcmp(reason,"ButtonColor") == 0) {
		m_ButtonColor.setRgba(rGrp.GetUnsigned(reason, QColor(226,233,239,128).rgba()));
	} else if (strcmp(reason,"CubeSize") == 0) {
		m_CubeWidgetSize = (rGrp.GetInt(reason, 132));
	}
}

char* NaviCubeImplementation::enum2str(int e) {
	switch (e) {
	default : return "???";
	case TEX_FRONT : return "TEX_FRONT";
	case TEX_REAR: return "TEX_REAR";
	case TEX_TOP: return "TEX_TOP";
	case TEX_BOTTOM: return "TEX_BOTTOM";
	case TEX_RIGHT : return "TEX_RIGHT";
	case TEX_LEFT: return "TEX_LEFT";
	case TEX_BACK_FACE: return "TEX_BACK_FACE";
	case TEX_FRONT_FACE: return "TEX_FRONT_FACE";
	case TEX_CORNER_FACE: return "TEX_CORNER_FACE";
	case TEX_BOTTOM_RIGHT_REAR: return "TEX_BOTTOM_RIGHT_REAR";
	case TEX_BOTTOM_FRONT_RIGHT: return "TEX_BOTTOM_FRONT_RIGHT";
	case TEX_BOTTOM_LEFT_FRONT: return "TEX_BOTTOM_LEFT_FRONT";
	case TEX_BOTTOM_REAR_LEFT: return "TEX_BOTTOM_REAR_LEFT";
	case TEX_TOP_RIGHT_FRONT: return "TEX_TOP_RIGHT_FRONT";
	case TEX_TOP_FRONT_LEFT: return "TEX_TOP_FRONT_LEFT";
	case TEX_TOP_LEFT_REAR: return "TEX_TOP_LEFT_REAR";
	case TEX_TOP_REAR_RIGHT: return "TEX_TOP_REAR_RIGHT";
	case TEX_ARROW_NORTH: return "TEX_ARROW_NORTH";
	case TEX_ARROW_SOUTH: return "TEX_ARROW_SOUTH";
	case TEX_ARROW_EAST: return "TEX_ARROW_EAST";
	case TEX_ARROW_WEST: return "TEX_ARROW_WEST";
	case TEX_ARROW_RIGHT: return "TEX_ARROW_RIGHT";
	case TEX_ARROW_LEFT: return "TEX_ARROW_LEFT";
	case TEX_VIEW_MENU_ICON : return "TEX_VIEW_MENU_ICON";
	case TEX_VIEW_MENU_FACE: return "TEX_VIEW_MENU";
	}
}

GLuint NaviCubeImplementation::createCubeFaceTex(QtGLWidget* gl, float gap, float radius, const char* text) {
	int texSize = m_CubeWidgetSize * m_OverSample;
	int gapi = texSize * gap;
	int radiusi = texSize * radius;
	QImage image(texSize, texSize, QImage::Format_ARGB32);
	image.fill(qRgba(255, 255, 255, 0));
	QPainter paint;
	paint.begin(&image);

	if (text) {
		ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/NaviCube");
		paint.setPen(Qt::white);
		QFont sansFont(str("Helvetica"), 0.18 * texSize);
		QString fontString = QString::fromUtf8((hGrp->GetASCII("FontString")).c_str());
		if (fontString.isEmpty()) {
			// Improving readability
			sansFont.setWeight(hGrp->GetInt("FontWeight", 87));
			sansFont.setStretch(hGrp->GetInt("FontStretch", 62));
		}
		else {
			sansFont.fromString(fontString);
		}
		// Override fromString
		if (hGrp->GetInt("FontWeight") > 0) {
			sansFont.setWeight(hGrp->GetInt("FontWeight"));
		}
		if (hGrp->GetInt("FontStretch") > 0) {
			sansFont.setStretch(hGrp->GetInt("FontStretch"));
		}
		paint.setFont(sansFont);
		paint.drawText(QRect(0, 0, texSize, texSize), Qt::AlignCenter,qApp->translate("Gui::NaviCube",text));
	}
	else {
		QPainterPath path;
		path.addRoundedRect(QRectF(gapi, gapi, texSize - 2 * gapi, texSize - 2 * gapi), radiusi, radiusi);
		paint.fillPath(path, Qt::white);
	}

	paint.end();
#if !defined(HAVE_QT5_OPENGL)
	return gl->bindTexture(image);
#else
    Q_UNUSED(gl);
    QOpenGLTexture *texture = new QOpenGLTexture(image.mirrored());
    m_glTextures.push_back(texture);
    texture->setMinificationFilter(QOpenGLTexture::Nearest);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);
    return texture->textureId();
#endif
}


GLuint NaviCubeImplementation::createButtonTex(QtGLWidget* gl, int button) {
	int texSize = m_CubeWidgetSize * m_OverSample;
	QImage image(texSize, texSize, QImage::Format_ARGB32);
	image.fill(qRgba(255, 255, 255, 0));
	QPainter painter;
	painter.begin(&image);

	QTransform transform;
	transform.translate(texSize / 2, texSize / 2);
	transform.scale(texSize / 2, texSize / 2);
	painter.setTransform(transform);

	QPainterPath path;

	float as1 = 0.18f; // arrow size
	float as3 = as1 / 3;

	switch (button) {
	default:
		break;
	case TEX_ARROW_RIGHT:
	case TEX_ARROW_LEFT: {
		QRectF r(-1.00, -1.00, 2.00, 2.00);
		QRectF r0(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r1(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r2(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r3(r);
		r.adjust(as3, as3, -as3, -as3);
		QRectF r4(r);

		float a0 = 72;
		float a1 = 45;
		float a2 = 32;

		if (TEX_ARROW_LEFT == button) {
			a0 = 180 - a0;
			a1 = 180 - a1;
			a2 = 180 - a2;
		}

		path.arcMoveTo(r0, a1);
		QPointF p0 = path.currentPosition();

		path.arcMoveTo(r2, a2);
		QPointF p1 = path.currentPosition();

		path.arcMoveTo(r4, a1);
		QPointF p2 = path.currentPosition();

		path.arcMoveTo(r1, a0);
		path.arcTo(r1, a0, -(a0 - a1));
		path.lineTo(p0);
		path.lineTo(p1);
		path.lineTo(p2);
		path.arcTo(r3, a1, +(a0 - a1));
		break;
	}
	case TEX_ARROW_EAST: {
		path.moveTo(1, 0);
		path.lineTo(1 - as1, +as1);
		path.lineTo(1 - as1, -as1);
		break;
	}
	case TEX_ARROW_WEST: {
		path.moveTo(-1, 0);
		path.lineTo(-1 + as1, -as1);
		path.lineTo(-1 + as1, +as1);
		break;
	}
	case TEX_ARROW_SOUTH: {
		path.moveTo(0, 1);
		path.lineTo(-as1,1 - as1);
		path.lineTo(+as1,1 - as1);
		break;
	}
	case TEX_ARROW_NORTH: {
		path.moveTo(0, -1);
		path.lineTo(+as1,-1 + as1);
		path.lineTo(-as1,-1 + as1);
		break;
	}
	}

	painter.fillPath(path, Qt::white);

	painter.end();
	//image.save(str(enum2str(button))+str(".png"));

#if !defined(HAVE_QT5_OPENGL)
	return gl->bindTexture(image);
#else
    Q_UNUSED(gl);
    QOpenGLTexture *texture = new QOpenGLTexture(image.mirrored());
    m_glTextures.push_back(texture);
    texture->setMinificationFilter(QOpenGLTexture::Nearest);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);
    return texture->textureId();
#endif
}

GLuint NaviCubeImplementation::createMenuTex(QtGLWidget* gl, bool forPicking) {
	int texSize = m_CubeWidgetSize * m_OverSample;
	QImage image(texSize, texSize, QImage::Format_ARGB32);
	image.fill(qRgba(0, 0, 0, 0));
	QPainter painter;
	painter.begin(&image);

	QTransform transform;
	transform.translate(texSize * 12 / 16, texSize * 13 / 16);
	transform.scale(texSize/200.0,texSize/200.0); // 200 == size at which this was designed
	painter.setTransform(transform);

	QPainterPath path;

	if (forPicking) {
		path.addRoundedRect(-25,-8,75,45,6,6);
		painter.fillPath(path, Qt::white);
	}
	else {
		// top
		path.moveTo(0,0);
		path.lineTo(15,5);
		path.lineTo(0,10);
		path.lineTo(-15,5);

		painter.fillPath(path, QColor(240,240,240));

		// left
		QPainterPath path2;
		path2.lineTo(0,10);
		path2.lineTo(-15,5);
		path2.lineTo(-15,25);
		path2.lineTo(0,30);
		painter.fillPath(path2, QColor(190,190,190));

		// right
		QPainterPath path3;
		path3.lineTo(0,10);
		path3.lineTo(15,5);
		path3.lineTo(15,25);
		path3.lineTo(0,30);
		painter.fillPath(path3, QColor(220,220,220));

		// outline
		QPainterPath path4;
		path4.moveTo(0,0);
		path4.lineTo(15,5);
		path4.lineTo(15,25);
		path4.lineTo(0,30);
		path4.lineTo(-15,25);
		path4.lineTo(-15,5);
		path4.lineTo(0,0);
		painter.strokePath(path4, QColor(128,128,128));

		// menu triangle
		QPainterPath path5;
		path5.moveTo(20,10);
		path5.lineTo(40,10);
		path5.lineTo(30,20);
		path5.lineTo(20,10);
		painter.fillPath(path5, QColor(64,64,64));
		}
	painter.end();
#if !defined(HAVE_QT5_OPENGL)
	return gl->bindTexture(image);
#else
    Q_UNUSED(gl);
    QOpenGLTexture *texture = new QOpenGLTexture(image.mirrored());
    m_glTextures.push_back(texture);
    texture->setMinificationFilter(QOpenGLTexture::Nearest);
    texture->setMagnificationFilter(QOpenGLTexture::Linear);
    return texture->textureId();
#endif
}



void NaviCubeImplementation::addFace(const Vector3f& x, const Vector3f& z, int frontTex, int backTex, int pickTex, int pickId,bool text) {
	Vector3f y = x.cross(-z);
	y = y / y.norm() * x.norm();

	int t = m_VertexArray.size();

	m_VertexArray.push_back(z - x - y);
	m_TextureCoordArray.emplace_back(0, 0);
	m_VertexArray.push_back(z + x - y);
	m_TextureCoordArray.emplace_back(1, 0);
	m_VertexArray.push_back(z + x + y);
	m_TextureCoordArray.emplace_back(1, 1);
	m_VertexArray.push_back(z - x + y);
	m_TextureCoordArray.emplace_back(0, 1);

	// TEX_TOP, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_TOP
	// TEX_TOP 			frontTex,
	// TEX_BACK_FACE 	backTex
	// TEX_FRONT_FACE	pickTex,
	// TEX_TOP 			pickId
	Face* ff = new Face(
		m_IndexArray.size(),
		4,
		m_Textures[pickTex],
		pickId,
		m_Textures[pickTex],
		m_FrontFaceColor,
		1);
	m_Faces.push_back(ff);

	if (text) {
		Face* ft = new Face(
			m_IndexArray.size(),
			4,
			m_Textures[frontTex],
			pickId,
			m_Textures[pickTex],
			m_TextColor,
			2);
		m_Faces.push_back(ft);

	}

	for (int i = 0; i < 4; i++)
		m_IndexArray.push_back(t + i);

	Face* bf = new Face(
		m_IndexArray.size(),
		4,
		m_Textures[backTex],
		pickId,
		m_Textures[backTex],
		m_BackFaceColor,
		0
		);

	m_Faces.push_back(bf);

	for (int i = 0; i < 4; i++)
		m_IndexArray.push_back(t + 4 - 1 - i);
}

void NaviCubeImplementation::initNaviCube(QtGLWidget* gl) {
	Vector3f x(1, 0, 0);
	Vector3f y(0, 1, 0);
	Vector3f z(0, 0, 1);

	float cs, sn;
	cs = cos(90 * M_PI / 180);
	sn = sin(90 * M_PI / 180);
	Matrix3f r90x;
	r90x << 1, 0, 0,
			0, cs, -sn,
			0, sn, cs;

	Matrix3f r90z;
	r90z << cs, sn, 0,
			-sn, cs, 0,
			0, 0, 1;

	cs = cos(45 * M_PI / 180);
	sn = sin(45 * M_PI / 180);
	Matrix3f r45z;
	r45z << cs, sn, 0,
			-sn, cs, 0,
			0, 0, 1;

	cs = cos(atan(sqrt(2.0)));
	sn = sin(atan(sqrt(2.0)));
	Matrix3f r45x;
	r45x << 1, 0, 0,
			0, cs, -sn,
			0, sn, cs;

	m_Textures[TEX_CORNER_FACE] = createCubeFaceTex(gl, 0, 0.5f, NULL);
	m_Textures[TEX_BACK_FACE] = createCubeFaceTex(gl, 0.02f, 0.3f, NULL);

    vector<string> labels = NaviCubeImplementation::m_labels;

    if (labels.size() != 6) {
        labels.clear();
        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/NaviCube");
        labels.push_back(hGrp->GetASCII("TextFront", "FRONT"));
        labels.push_back(hGrp->GetASCII("TextRear", "REAR"));
        labels.push_back(hGrp->GetASCII("TextTop", "TOP"));
        labels.push_back(hGrp->GetASCII("TextBottom", "BOTTOM"));
        labels.push_back(hGrp->GetASCII("TextRight", "RIGHT"));
        labels.push_back(hGrp->GetASCII("TextLeft", "LEFT"));
    }

	float gap = 0.12f;
	float radius = 0.12f;

	m_Textures[TEX_FRONT] = createCubeFaceTex(gl, gap, radius, labels[0].c_str());
	m_Textures[TEX_REAR] = createCubeFaceTex(gl, gap, radius, labels[1].c_str());
	m_Textures[TEX_TOP] = createCubeFaceTex(gl, gap, radius, labels[2].c_str());
	m_Textures[TEX_BOTTOM] = createCubeFaceTex(gl, gap, radius, labels[3].c_str());
	m_Textures[TEX_RIGHT] = createCubeFaceTex(gl, gap, radius, labels[4].c_str());
	m_Textures[TEX_LEFT] = createCubeFaceTex(gl, gap, radius, labels[5].c_str());

	m_Textures[TEX_FRONT_FACE] = createCubeFaceTex(gl, gap, radius, NULL);

	m_Textures[TEX_ARROW_NORTH] = createButtonTex(gl, TEX_ARROW_NORTH);
	m_Textures[TEX_ARROW_SOUTH] = createButtonTex(gl, TEX_ARROW_SOUTH);
	m_Textures[TEX_ARROW_EAST] = createButtonTex(gl, TEX_ARROW_EAST);
	m_Textures[TEX_ARROW_WEST] = createButtonTex(gl, TEX_ARROW_WEST);
	m_Textures[TEX_ARROW_LEFT] = createButtonTex(gl, TEX_ARROW_LEFT);
	m_Textures[TEX_ARROW_RIGHT] = createButtonTex(gl, TEX_ARROW_RIGHT);
	m_Textures[TEX_VIEW_MENU_ICON] = createMenuTex(gl, false);
	m_Textures[TEX_VIEW_MENU_FACE] = createMenuTex(gl, true);

			// front,back,pick,pickid
	addFace(x, z, TEX_TOP, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_TOP,true);
	x = r90x * x;
	z = r90x * z;
	addFace(x, z, TEX_FRONT, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_FRONT,true);
	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_LEFT, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_LEFT,true);
	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_REAR, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_REAR,true);
	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_RIGHT, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_RIGHT,true);
	x = r90x * r90z * x;
	z = r90x * r90z * z;
	addFace(x, z, TEX_BOTTOM, TEX_BACK_FACE, TEX_FRONT_FACE, TEX_BOTTOM,true);

	z = r45z * r45x * z;
	x = r45z * r45x * x;

	x *= 0.23f; // corner face size
	z *= 1.43f; // corner face position

	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_BOTTOM_RIGHT_REAR);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_BOTTOM_FRONT_RIGHT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_BOTTOM_LEFT_FRONT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_BOTTOM_REAR_LEFT);

	x = r90x * r90x * r90z * x;
	z = r90x * r90x * r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_TOP_RIGHT_FRONT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_TOP_FRONT_LEFT);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_TOP_LEFT_REAR);

	x = r90z * x;
	z = r90z * z;
	addFace(x, z, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_CORNER_FACE, TEX_TOP_REAR_RIGHT);

	m_Buttons.push_back(TEX_ARROW_NORTH);
	m_Buttons.push_back(TEX_ARROW_SOUTH);
	m_Buttons.push_back(TEX_ARROW_EAST);
	m_Buttons.push_back(TEX_ARROW_WEST);
	m_Buttons.push_back(TEX_ARROW_LEFT);
	m_Buttons.push_back(TEX_ARROW_RIGHT);

	m_PickingFramebuffer = new QtGLFramebufferObject(2*m_CubeWidgetSize,2* m_CubeWidgetSize, QtGLFramebufferObject::CombinedDepthStencil);
}

void NaviCubeImplementation::drawNaviCube() {
	glViewport(m_CubeWidgetPosX-m_CubeWidgetSize/2, m_CubeWidgetPosY-m_CubeWidgetSize/2, m_CubeWidgetSize, m_CubeWidgetSize);
	drawNaviCube(false);
}

void NaviCubeImplementation::createContextMenu(const std::vector<std::string>& cmd) {
    CommandManager &rcCmdMgr = Application::Instance->commandManager();
    m_Menu->clear();

    for (vector<string>::const_iterator i=cmd.begin(); i!=cmd.end(); i++) {
        Command* cmd = rcCmdMgr.getCommandByName(i->c_str());
        if (cmd)
            cmd->addTo(m_Menu);
    }
}

void NaviCubeImplementation::handleResize() {
	SbVec2s view = m_View3DInventorViewer->getSoRenderManager()->getSize();
	if ((m_PrevWidth != view[0]) || (m_PrevHeight != view[1])) {
		if ((m_PrevWidth > 0) && (m_PrevHeight > 0)) {
			// maintain position relative to closest edge
			if (m_CubeWidgetPosX > m_PrevWidth / 2)
				m_CubeWidgetPosX = view[0] - (m_PrevWidth - m_CubeWidgetPosX);
			if (m_CubeWidgetPosY > m_PrevHeight / 2)
				m_CubeWidgetPosY = view[1] - (m_PrevHeight - m_CubeWidgetPosY);
		}
		else { // initial position
			ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/NaviCube");
			int m_CubeWidgetOffsetX = hGrp->GetInt("OffsetX", 0);
			int m_CubeWidgetOffsetY = hGrp->GetInt("OffsetY", 0);
			switch (m_Corner) {
			case NaviCube::TopLeftCorner:
				m_CubeWidgetPosX = m_CubeWidgetSize*1.1 / 2 + m_CubeWidgetOffsetX;
				m_CubeWidgetPosY = view[1] - m_CubeWidgetSize*1.1 / 2 - m_CubeWidgetOffsetY;
				break;
			case NaviCube::TopRightCorner:
				m_CubeWidgetPosX = view[0] - m_CubeWidgetSize*1.1 / 2 - m_CubeWidgetOffsetX;
				m_CubeWidgetPosY = view[1] - m_CubeWidgetSize*1.1 / 2 - m_CubeWidgetOffsetY;
				break;
			case NaviCube::BottomLeftCorner:
				m_CubeWidgetPosX = m_CubeWidgetSize*1.1 / 2 + m_CubeWidgetOffsetX;
				m_CubeWidgetPosY = m_CubeWidgetSize*1.1 / 2 + m_CubeWidgetOffsetY;
				break;
			case NaviCube::BottomRightCorner:
				m_CubeWidgetPosX = view[0] - m_CubeWidgetSize*1.1 / 2 - m_CubeWidgetOffsetX;
				m_CubeWidgetPosY = m_CubeWidgetSize*1.1 / 2 + m_CubeWidgetOffsetY;
				break;
			}
		}
		m_PrevWidth = view[0];
		m_PrevHeight = view[1];
		m_View3DInventorViewer->getSoRenderManager()->scheduleRedraw();

	}

}

void NaviCubeImplementation::drawNaviCube(bool pickMode) {
	// initializes stuff here when we actually have a context
    // FIXME actually now that we have Qt5, we could probably do this earlier (as we do not need the opengl context)
	if (!m_NaviCubeInitialised) {
		QtGLWidget* gl = static_cast<QtGLWidget*>(m_View3DInventorViewer->viewport());
		if (gl == NULL)
			return;
		initNaviCube(gl);
		m_NaviCubeInitialised = true;
	}

	SoCamera* cam = m_View3DInventorViewer->getSoRenderManager()->getCamera();

	if (!cam)
		return;

	handleResize();

	// Store GL state.
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	GLfloat depthrange[2];
	glGetFloatv(GL_DEPTH_RANGE, depthrange);
	GLdouble projectionmatrix[16];
	glGetDoublev(GL_PROJECTION_MATRIX, projectionmatrix);

	glDepthMask(GL_TRUE);
	glDepthRange(0.0, 1.0);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glLineWidth(2.0);

	glDisable(GL_LIGHTING);
	//glDisable(GL_BLEND);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//glTexEnvf(GL_TEXTURE_2D, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glShadeModel(GL_SMOOTH);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	glAlphaFunc( GL_GREATER, 0.25);
	glEnable( GL_ALPHA_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	const float NEARVAL = 0.1f;
	const float FARVAL = 10.0f;
	const float dim = NEARVAL * float(tan(M_PI / 8.0))*1.2;
	glFrustum(-dim, dim, -dim, dim, NEARVAL, FARVAL);

	SbMatrix mx;
	mx = cam->orientation.getValue();

	mx = mx.inverse();
	mx[3][2] = -5.0;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf((float*) mx);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if (pickMode) {
		glDisable(GL_BLEND);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glShadeModel(GL_FLAT);
		glDisable(GL_DITHER);
		glDisable(GL_POLYGON_SMOOTH);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	glClear(GL_DEPTH_BUFFER_BIT);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, (void*) m_VertexArray.data());
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, m_TextureCoordArray.data());

	if (!pickMode) {
		// Draw the axes
		ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/NaviCube");
		bool ShowCS = hGrp->GetBool("ShowCS", 1);
		if (ShowCS) {
			glDisable(GL_TEXTURE_2D);
			float a=1.1f;

			static GLubyte xbmp[] = { 0x11,0x11,0x0a,0x04,0x0a,0x11,0x11 };
			glColor3f(1, 0, 0);
			glBegin(GL_LINES);
			glVertex3f(-1.1f, -1.1f, -1.1f);
			glVertex3f(+0.5f, -1.1f, -1.1f);
			glEnd();
			glRasterPos3d(a, -a, -a);
			glBitmap(8, 7, 0, 0, 0, 0, xbmp);

			static GLubyte ybmp[] = { 0x04,0x04,0x04,0x04,0x0a,0x11,0x11 };
			glColor3f(0, 1, 0);
			glBegin(GL_LINES);
			glVertex3f(-1.1f, -1.1f, -1.1f);
			glVertex3f(-1.1f, +0.5f, -1.1f);
			glEnd();
			glRasterPos3d( -a, a, -a);
			glBitmap(8, 7, 0, 0, 0, 0, ybmp);

			static GLubyte zbmp[] = { 0x1f,0x10,0x08,0x04,0x02,0x01,0x1f };
			glColor3f(0, 0, 1);
			glBegin(GL_LINES);
			glVertex3f(-1.1f, -1.1f, -1.1f);
			glVertex3f(-1.1f, -1.1f, +0.5f);
			glEnd();
			glRasterPos3d( -a, -a, a);
			glBitmap(8, 7, 0, 0, 0, 0, zbmp);

			glEnable(GL_TEXTURE_2D);
		}
	}

	// Draw the cube faces
	if (pickMode) {
		for (vector<Face*>::iterator f = m_Faces.begin(); f != m_Faces.end(); f++) {
			glColor3ub((*f)->m_PickId, 0, 0);
			glBindTexture(GL_TEXTURE_2D, (*f)->m_PickTextureId);
			glDrawElements(GL_TRIANGLE_FAN, (*f)->m_VertexCount, GL_UNSIGNED_BYTE, (void*) &m_IndexArray[(*f)->m_FirstVertex]);
		}
	}
	else {
		for (int pass = 0; pass < 3 ; pass++) {
			for (vector<Face*>::iterator f = m_Faces.begin(); f != m_Faces.end(); f++) {
				//if (pickMode) { // pick should not be drawn in tree passes
				//	glColor3ub((*f)->m_PickId, 0, 0);
				//	glBindTexture(GL_TEXTURE_2D, (*f)->m_PickTextureId);
				//} else {
					if (pass != (*f)->m_RenderPass)
						continue;
					QColor& c = (m_HiliteId == (*f)->m_PickId) && (pass < 2) ? m_HiliteColor : (*f)->m_Color;
					glColor4f(c.redF(), c.greenF(), c.blueF(),c.alphaF());
					glBindTexture(GL_TEXTURE_2D, (*f)->m_TextureId);
				//}
				glDrawElements(GL_TRIANGLE_FAN, (*f)->m_VertexCount, GL_UNSIGNED_BYTE, (void*) &m_IndexArray[(*f)->m_FirstVertex]);
			}
		}
	}


	// Draw the rotate buttons
	glEnable(GL_CULL_FACE);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	for (vector<int>::iterator b = m_Buttons.begin(); b != m_Buttons.end(); b++) {
		if (pickMode)
			glColor3ub(*b, 0, 0);
		else {
			QColor& c = (m_HiliteId ==(*b)) ? m_HiliteColor : m_ButtonColor;
			glColor4f(c.redF(), c.greenF(), c.blueF(), c.alphaF());
		}
		glBindTexture(GL_TEXTURE_2D, m_Textures[*b]);

		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex3f(0.0f, 1.0f, 0.0f);
		glTexCoord2f(1, 0);
		glVertex3f(1.0f, 1.0f, 0.0f);
		glTexCoord2f(1, 1);
		glVertex3f(1.0f, 0.0f, 0.0f);
		glTexCoord2f(0, 1);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glEnd();
	}

	// Draw the view menu icon
	if (pickMode) {
		glColor3ub(TEX_VIEW_MENU_FACE, 0, 0);
		glBindTexture(GL_TEXTURE_2D, m_Textures[TEX_VIEW_MENU_FACE]);
	}
	else {
		if (m_HiliteId == TEX_VIEW_MENU_FACE) {
			QColor& c = m_HiliteColor;
			glColor4f(c.redF(), c.greenF(), c.blueF(),c.alphaF());
			glBindTexture(GL_TEXTURE_2D, m_Textures[TEX_VIEW_MENU_FACE]);

			glBegin(GL_QUADS); // DO THIS WITH VERTEX ARRAYS
			glTexCoord2f(0, 0);
			glVertex3f(0.0f, 1.0f, 0.0f);
			glTexCoord2f(1, 0);
			glVertex3f(1.0f, 1.0f, 0.0f);
			glTexCoord2f(1, 1);
			glVertex3f(1.0f, 0.0f, 0.0f);
			glTexCoord2f(0, 1);
			glVertex3f(0.0f, 0.0f, 0.0f);
			glEnd();
		}

		QColor& c = m_ButtonColor;
		glColor4f(c.redF(), c.greenF(), c.blueF(), c.alphaF());
		glBindTexture(GL_TEXTURE_2D, m_Textures[TEX_VIEW_MENU_ICON]);
	}

	glBegin(GL_QUADS); // FIXME do this with vertex arrays
	glTexCoord2f(0, 0);
	glVertex3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(1, 0);
	glVertex3f(1.0f, 1.0f, 0.0f);
	glTexCoord2f(1, 1);
	glVertex3f(1.0f, 0.0f, 0.0f);
	glTexCoord2f(0, 1);
	glVertex3f(0.0f, 0.0f, 0.0f);
	glEnd();


	glPopMatrix();

	// Restore original state.

	glDepthRange(depthrange[0], depthrange[1]);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(projectionmatrix);

	glPopAttrib();
}

int NaviCubeImplementation::pickFace(short x, short y) {
	GLubyte pixels[4] = {0};
	if (m_PickingFramebuffer) {
		m_PickingFramebuffer->bind();

		glViewport(0, 0, 2*m_CubeWidgetSize,2* m_CubeWidgetSize);
		glLoadIdentity();

		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		drawNaviCube(true);

		glFinish();

		glReadPixels(2*(x - (m_CubeWidgetPosX-m_CubeWidgetSize/2)), 2*(y - (m_CubeWidgetPosY-m_CubeWidgetSize/2)), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixels);
		m_PickingFramebuffer->release();

		//QImage image = m_PickingFramebuffer->toImage();
		//image.save(QLatin1String("pickimage.png"));
	}
	return pixels[3] == 255 ? pixels[0] : 0;
}

bool NaviCubeImplementation::mousePressed(short x, short y) {
	m_MouseDown = true;
	m_Dragging = false;
	m_MightDrag = inDragZone(x, y);
	int pick = pickFace(x, y);
	// cerr << enum2str(pick) << endl;
	setHilite(pick);
	return pick != 0;
}

void NaviCubeImplementation::setView(float rotZ,float rotX) {
	SbRotation rz, rx, t;
	rz.setValue(SbVec3f(0, 0, 1), rotZ * M_PI / 180);
	rx.setValue(SbVec3f(1, 0, 0), rotX * M_PI / 180);
	m_View3DInventorViewer->setCameraOrientation(rx * rz);
}

void NaviCubeImplementation::rotateView(int axis,float rotAngle) {
	SbRotation viewRot = m_View3DInventorViewer->getCameraOrientation();

	SbVec3f up;
	viewRot.multVec(SbVec3f(0,1,0),up);

	SbVec3f out;
	viewRot.multVec(SbVec3f(0,0,1),out);

	SbVec3f& u = up;
	SbVec3f& o = out;
	SbVec3f right  (u[1]*o[2]-u[2]*o[1], u[2]*o[0]-u[0]*o[2], u[0]*o[1]-u[1]*o[0]);

	SbVec3f direction;
	switch (axis) {
	default :
		return;
	case DIR_UP :
		direction = up;
		break;
	case DIR_OUT :
		direction = out;
		break;
	case DIR_RIGHT :
		direction = right;
		break;
	}

	SbRotation rot(direction, -rotAngle*M_PI/180.0);
	SbRotation newViewRot = viewRot * rot;
	m_View3DInventorViewer->setCameraOrientation(newViewRot);

}

void NaviCubeImplementation::handleMenu() {
	m_Menu->exec(QCursor::pos());
}

bool NaviCubeImplementation::mouseReleased(short x, short y) {
	setHilite(0);
	m_MouseDown = false;
	if (!m_Dragging) {
		float rot = 45 ; //30;
		float tilt = 90-54.7356f ; //30; // 90 + deg(asin(-sqrt(1.0/3.0)))
		int pick = pickFace(x, y);

		ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/View");
		long step = Base::clamp(hGrp->GetInt("NaviStepByTurn",8), 4L, 36L);
		float rotStepAngle = 360.0f/step;

		switch (pick) {
		default:
			return false;
			break;
		case TEX_FRONT:
			setView(0, 90);
			break;
		case TEX_REAR:
			setView(180, 90);
			break;
		case TEX_LEFT:
			setView(270,90);
			break;
		case TEX_RIGHT:
			setView(90,90);
			break;
		case TEX_TOP:
			setView(0,0);
			break;
		case TEX_BOTTOM:
			setView(0,180);
			break;
		case TEX_BOTTOM_LEFT_FRONT:
			setView(rot - 90, 90 + tilt);
			break;
		case TEX_BOTTOM_FRONT_RIGHT:
			setView(90 + rot - 90, 90 + tilt);
			break;
		case TEX_BOTTOM_RIGHT_REAR:
			setView(180 + rot - 90, 90 + tilt);
			break;
		case TEX_BOTTOM_REAR_LEFT:
			setView(270 + rot - 90, 90 + tilt);
			break;
		case TEX_TOP_RIGHT_FRONT:
			setView(rot, 90 - tilt);
			break;
		case TEX_TOP_FRONT_LEFT:
			setView(rot - 90, 90 - tilt);
			break;
		case TEX_TOP_LEFT_REAR:
			setView(rot - 180, 90 - tilt);
			break;
		case TEX_TOP_REAR_RIGHT:
			setView(rot - 270, 90 - tilt);
			break;
		case TEX_ARROW_LEFT :
			rotateView(DIR_OUT,rotStepAngle);
			break;
		case TEX_ARROW_RIGHT :
			rotateView(DIR_OUT,-rotStepAngle);
			break;
		case TEX_ARROW_WEST :
			rotateView(DIR_UP,-rotStepAngle);
			break;
		case TEX_ARROW_EAST :
			rotateView(DIR_UP,rotStepAngle);
			break;
		case TEX_ARROW_NORTH :
			rotateView(DIR_RIGHT,-rotStepAngle);
			break;
		case TEX_ARROW_SOUTH :
			rotateView(DIR_RIGHT,rotStepAngle);
			break;
		case TEX_VIEW_MENU_FACE :
			handleMenu();
			break;
		}
	}
	return true;
}


void NaviCubeImplementation::setHilite(int hilite) {
	if (hilite != m_HiliteId) {
		m_HiliteId = hilite;
		//cerr << "m_HiliteFace " << m_HiliteId << endl;
		m_View3DInventorViewer->getSoRenderManager()->scheduleRedraw();
	}
}

bool NaviCubeImplementation::inDragZone(short x, short y) {
	int dx = x - m_CubeWidgetPosX;
	int dy = y - m_CubeWidgetPosY;
	int limit = m_CubeWidgetSize/4;
	return abs(dx)<limit && abs(dy)<limit;
}


bool NaviCubeImplementation::mouseMoved(short x, short y) {
	setHilite(pickFace(x, y));

	if (m_MouseDown) {
		if (m_MightDrag && !m_Dragging && !inDragZone(x, y))
			m_Dragging = true;
		if (m_Dragging) {
			setHilite(0);
			m_CubeWidgetPosX = x;
			m_CubeWidgetPosY = y;
			this->m_View3DInventorViewer->getSoRenderManager()->scheduleRedraw();
			return true;
		}
	}
	return false;
}

bool NaviCubeImplementation::processSoEvent(const SoEvent* ev) {
    short x, y;
    ev->getPosition().getValue(x, y);
    // FIXME find out why do we need to hack the cursor position to get
    // 2019-02-17
    // The above comment is truncated; don't know what it's about
    // The two hacked lines changing the cursor position are responsible for
    // parts of the navigational cluster not being active.
    // Commented them out and everything seems to be working
//    y += 4;
//    x -= 2;
	if (ev->getTypeId().isDerivedFrom(SoMouseButtonEvent::getClassTypeId())) {
		const SoMouseButtonEvent* mbev = static_cast<const SoMouseButtonEvent*>(ev);
		if (mbev->isButtonPressEvent(mbev, SoMouseButtonEvent::BUTTON1))
			return mousePressed(x, y);
		if (mbev->isButtonReleaseEvent(mbev, SoMouseButtonEvent::BUTTON1))
			return mouseReleased(x, y);
	}
	if (ev->getTypeId().isDerivedFrom(SoLocation2Event::getClassTypeId()))
		return mouseMoved(x, y);
	return false;
}


QString NaviCubeImplementation::str(char* str) {
	return QString::fromLatin1(str);
}

void NaviCube::setNaviCubeCommands(const std::vector<std::string>& cmd)
{
    NaviCubeImplementation::m_commands = cmd;
}

void NaviCube::setNaviCubeLabels(const std::vector<std::string>& labels)
{
    NaviCubeImplementation::m_labels = labels;
}



DEF_3DV_CMD(ViewIsometricCmd)
ViewIsometricCmd::ViewIsometricCmd()
  : Command("ViewIsometricCmd")
{
    sGroup        = QT_TR_NOOP("");
    sMenuText     = QT_TR_NOOP("Isometric");
    sToolTipText  = QT_TR_NOOP("Set NaviCube to Isometric mode");
    sWhatsThis    = "";
    sStatusTip    = sToolTipText;
    sPixmap       = "";
    sAccel        = "";
    eType         = Alter3DView;
}

void ViewIsometricCmd::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Command::doCommand(Command::Gui,"Gui.activeDocument().activeView().viewIsometric()");
}

DEF_3DV_CMD(ViewOrthographicCmd)
ViewOrthographicCmd::ViewOrthographicCmd()
  : Command("ViewOrthographicCmd")
{
    sGroup        = QT_TR_NOOP("");
    sMenuText     = QT_TR_NOOP("Orthographic");
    sToolTipText  = QT_TR_NOOP("Set View to Orthographic mode");
    sWhatsThis    = "";
    sStatusTip    = sToolTipText;
    sPixmap       = "";
    sAccel        = "";
    eType         = Alter3DView;
}

void ViewOrthographicCmd::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Command::doCommand(Command::Gui,"Gui.activeDocument().activeView().setCameraType(\"Orthographic\")");
}

DEF_3DV_CMD(ViewPerspectiveCmd)

ViewPerspectiveCmd::ViewPerspectiveCmd()
  : Command("ViewPerspectiveCmd")
{
    sGroup        = QT_TR_NOOP("");
    sMenuText     = QT_TR_NOOP("Perspective");
    sToolTipText  = QT_TR_NOOP("Set View to Perspective mode");
    sWhatsThis    = "";
    sStatusTip    = sToolTipText;
    sPixmap       = "";
    sAccel        = "";
    eType         = Alter3DView;
}

void ViewPerspectiveCmd::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Command::doCommand(Command::Gui,"Gui.activeDocument().activeView().setCameraType(\"Perspective\")");
}

DEF_3DV_CMD(ViewZoomToFitCmd)

ViewZoomToFitCmd::ViewZoomToFitCmd()
  : Command("ViewZoomToFit")
{
    sGroup        = QT_TR_NOOP("");
    sMenuText     = QT_TR_NOOP("Zoom to fit");
    sToolTipText  = QT_TR_NOOP("Zoom so that model fills the view");
    sWhatsThis    = "";
    sStatusTip    = sToolTipText;
    sPixmap       = "";
    sAccel        = "";
    eType         = Alter3DView;
}

void ViewZoomToFitCmd::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Command::doCommand(Command::Gui, "Gui.SendMsgToActiveView(\"ViewFit\")");
}


QMenu* NaviCubeImplementation::createNaviCubeMenu() {
    QMenu* menu = new QMenu(getMainWindow());
    menu->setObjectName(str("NaviCube_Menu"));

    CommandManager &rcCmdMgr = Application::Instance->commandManager();
    static bool init = true;
    if (init) {
        init = false;
        rcCmdMgr.addCommand(new ViewOrthographicCmd);
        rcCmdMgr.addCommand(new ViewPerspectiveCmd);
        rcCmdMgr.addCommand(new ViewIsometricCmd);
        rcCmdMgr.addCommand(new ViewZoomToFitCmd);
    }

    vector<string> commands = NaviCubeImplementation::m_commands;
    if (commands.empty()) {
        commands.push_back("ViewOrthographicCmd");
        commands.push_back("ViewPerspectiveCmd");
        commands.push_back("ViewIsometricCmd");
        commands.push_back("Separator");
        commands.push_back("ViewZoomToFit");
    }

    for (vector<string>::iterator i=commands.begin(); i!=commands.end(); ++i) {
        if (*i == "Separator") {
            menu->addSeparator();
        }
        else {
            Command* cmd = rcCmdMgr.getCommandByName(i->c_str());
            if (cmd)
                cmd->addTo(menu);
        }
    }
    return menu;
}
