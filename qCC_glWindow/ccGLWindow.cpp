//##########################################################################
//#                                                                        #
//#                            CLOUDCOMPARE                                #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

//CCLib
#include <CCConst.h>

#include "ccShader.h"

//qCC
#include "ccGLWindow.h"
#include "ccGuiParameters.h"
#include "ccRenderingTools.h"

//qCC_db
#include <ccLog.h>
#include <ccHObject.h>
#include <ccBBox.h>
#include <cc2DLabel.h>
#include <ccGenericPointCloud.h>
#include <ccGenericMesh.h>
#include <ccTimer.h>
#include <ccSphere.h> //for the pivot symbol
#include <ccPolyline.h>
#include <ccPointCloud.h>
#include <ccColorRampShader.h>
#include <ccClipBox.h>
#include <ccBox.h>
#include <ccMaterialSet.h>

//CCFbo
#include <ccShader.h>
#include <ccGlFilter.h>
#include <ccFrameBufferObject.h>

//System
#include <string.h>
#include <math.h>
#include <algorithm>

#include "gb2312.h"

//Min and max zoom ratio (relative)
const float CC_GL_MAX_ZOOM_RATIO = 1.0e6f;
const float CC_GL_MIN_ZOOM_RATIO = 1.0e-6f;

//Vaious overlay elements dimensions
const float CC_DISPLAYED_TRIHEDRON_AXES_LENGTH = 25.0f;
const float CC_DISPLAYED_PIVOT_RADIUS_PERCENT = 0.8f; //percentage of the smallest screen dimension
const float CC_DISPLAYED_CUSTOM_LIGHT_LENGTH = 10.0f;
const float CC_DISPLAYED_CENTER_CROSS_LENGTH = 10.0f;

//Hot zone (interactors) dimensions
const int CC_HOT_ZONE_WIDTH = 270;
const int CC_HOT_ZONE_HEIGHT = 100;

//Max click duration for enabling picking mode (in ms)
const int CC_MAX_PICKING_CLICK_DURATION_MS = 200;

//invalid GL list index
const GLuint GL_INVALID_LIST_ID = (~0);

/*** Persistent settings ***/

static const char c_ps_groupName[]			= "ccGLWindow";
static const char c_ps_perspectiveView[]	= "perspectiveView";
static const char c_ps_objectMode[]			= "objectCenteredView";
static const char c_ps_sunLight[]			= "sunLightEnabled";
static const char c_ps_customLight[]		= "customLightEnabled";
static const char c_ps_pivotVisibility[]	= "pivotVisibility";

//Unique GL window ID
static int s_GlWindowNumber = 0;

ccGLWindow::ccGLWindow():
	m_uniqueID(++s_GlWindowNumber) //GL window unique ID
	, m_initialized(false)
	, m_trihedronGLList(GL_INVALID_LIST_ID)
	, m_pivotGLList(GL_INVALID_LIST_ID)
	, m_lastMouseOrientation(1,0,0)
	, m_currentMouseOrientation(1,0,0)
	, m_validModelviewMatrix(false)
	, m_validProjectionMatrix(false)
	, m_glWidth(0)
	, m_glHeight(0)
	, m_lodActivated(false)
	, m_shouldBeRefreshed(false)
	, m_cursorMoved(false)
	, m_unclosable(false)
	, m_interactionMode(TRANSFORM_CAMERA)
	, m_pickingMode(NO_PICKING)
	, m_captureMode(false)
	, m_captureModeZoomFactor(1.0f)
	, m_lastClickTime_ticks(0)
	, m_sunLightEnabled(true)
	, m_customLightEnabled(false)
	, m_embeddedIconsEnabled(false)
	, m_hotZoneActivated(false)
	, m_activeShader(0)
	, m_shadersEnabled(false)
	, m_fbo(0)
	, m_alwaysUseFBO(false)
	, m_updateFBO(true)
	, m_colorRampShader(0)
	, m_activeGLFilter(0)
	, m_glFiltersEnabled(false)
	, m_winDBRoot(0)
	, m_globalDBRoot(0) //external DB
	, m_pivotVisibility(PIVOT_SHOW_ON_MOVE)
	, m_pivotSymbolShown(false)
	, m_allowRectangularEntityPicking(true)
	, m_rectPickingPoly(0)
{
	entitySelectionChanged = NULL;
	entitiesSelectionChanged = NULL;
	pointPicked = NULL;
	viewMatRotated = NULL;
	cameraDisplaced = NULL;
	mouseWheelRotated = NULL;
	perspectiveStateChanged = NULL;
	baseViewMatChanged = NULL;
	pivotPointChanged = NULL;
	cameraPosChanged = NULL;
	translation = NULL;
	rotation = NULL;
	leftButtonClicked = NULL;
	rightButtonClicked = NULL;
	mouseMoved = NULL;
	buttonReleased = NULL;
	drawing3D = NULL;
	newLabel = NULL;
	newCube = NULL;

	m_lastMousePos.x = -1;
	m_lastMousePos.y = -1;

	//GL window own DB
	m_winDBRoot = new ccHObject(std::string("DB.3DView_") + std::to_string(m_uniqueID));

	//lights
	m_sunLightEnabled = true;
	m_sunLightPos[0] = 0.0f;
	m_sunLightPos[1] = 1.0f;
	m_sunLightPos[2] = 1.0f;
	m_sunLightPos[3] = 0.0f;

	m_customLightEnabled = false;
	m_customLightPos[0] = 0.0f;
	m_customLightPos[1] = 0.0f;
	m_customLightPos[2] = 0.0f;
	m_customLightPos[3] = 1.0f; //positional light

	//matrices
	m_params.viewMat.toIdentity();
	memset(m_viewMatd, 0, sizeof(double)*OPENGL_MATRIX_SIZE);
	memset(m_projMatd, 0, sizeof(double)*OPENGL_MATRIX_SIZE);

	//default modes
	setPickingMode(DEFAULT_PICKING);
	setInteractionMode(TRANSFORM_CAMERA);

	//embedded icons (point size, etc.)
	enableEmbeddedIcons(true);

	//auto-load previous perspective settings
	{
	  // TODO: make these settings persistent
		bool perspectiveView	= false;
		bool objectCenteredView	= true;
		m_sunLightEnabled		= true;
		m_customLightEnabled	= false;
		int pivotVisibility		= PIVOT_SHOW_ON_MOVE;

		//perspective
		if (!perspectiveView)
			ccLog::Print("[ccGLWindow] Persective is off by default");
		else
			ccLog::Print(std::string("[ccGLWindow] Persective is on by default  - ") + (objectCenteredView ? "object-centered" : "viewer-centered"));

		//pivot visibility
		switch(pivotVisibility)
		{
		case PIVOT_HIDE:
			setPivotVisibility(PIVOT_HIDE);
			break;
		case PIVOT_SHOW_ON_MOVE:
			setPivotVisibility(PIVOT_SHOW_ON_MOVE);
			break;
		case PIVOT_ALWAYS_SHOW:
			setPivotVisibility(PIVOT_ALWAYS_SHOW);
			break;
		}

		//apply saved parameters
		setPerspectiveState(perspectiveView, objectCenteredView);
		if (m_customLightEnabled)
			displayNewMessage("Warning: custom light is ON",ccGLWindow::LOWER_LEFT_MESSAGE,false,2,CUSTOM_LIGHT_STATE_MESSAGE);
		if (!m_sunLightEnabled)
			displayNewMessage("Warning: sun light is OFF",ccGLWindow::LOWER_LEFT_MESSAGE,false,2,SUN_LIGHT_STATE_MESSAGE);
	}
}

ccGLWindow::~ccGLWindow()
{
	makeCurrent();
	if (m_trihedronGLList != GL_INVALID_LIST_ID)
	{
		glDeleteLists(m_trihedronGLList,1);
		m_trihedronGLList = GL_INVALID_LIST_ID;
	}
	if (m_pivotGLList != GL_INVALID_LIST_ID)
	{
		glDeleteLists(m_pivotGLList,1);
		m_pivotGLList = GL_INVALID_LIST_ID;
	}

	if (m_winDBRoot)
		delete m_winDBRoot;

	if (m_rectPickingPoly)
		delete m_rectPickingPoly;

	if (m_activeGLFilter)
		delete m_activeGLFilter;

	if (m_colorRampShader)
		delete m_colorRampShader;

	if (m_activeShader)
		delete m_activeShader;

	if (m_fbo)
		delete m_fbo;
}

void ccGLWindow::initializeGL()
{
	if (m_initialized)
		return;

	//we init model view matrix with identity and store it into 'viewMat' and 'm_viewMatd'
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glGetFloatv(GL_MODELVIEW_MATRIX, m_params.viewMat.data());
	glGetDoublev(GL_MODELVIEW_MATRIX, m_viewMatd);

	//we emit the 'baseViewMatChanged' signal
	/*emit*/if (baseViewMatChanged) baseViewMatChanged(m_params.viewMat);

	//we init projection matrix with identity and push it on the stack
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//glGetFloatv(GL_PROJECTION_MATRIX, m_projMat.data());
	glGetDoublev(GL_PROJECTION_MATRIX, m_projMatd);

	//set viewport and visu. as invalid
	invalidateViewport();
	invalidateVisualization();

	//we initialize GLEW
	InitGLEW();

	//OpenGL version
	ccLog::Print("[3D View %i] GL version: %s",m_uniqueID,glGetString(GL_VERSION));

	//Shaders and other OpenGL extensions
	m_shadersEnabled = CheckShadersAvailability();
	if (!m_shadersEnabled)
	{
		//if no shader, no GL filter!
		ccLog::Warning("[3D View %i] Shaders and GL filters unavailable",m_uniqueID);
	}
	else
	{
		ccLog::Print("[3D View %i] Shaders available",m_uniqueID);

		m_glFiltersEnabled = CheckFBOAvailability();
		if (m_glFiltersEnabled)
		{
			ccLog::Print("[3D View %i] GL filters available",m_uniqueID);
			m_alwaysUseFBO = true;
		}
		else
		{
			ccLog::Warning("[3D View %i] GL filters unavailable (FBO not supported)",m_uniqueID);
		}

		//color ramp shader
		if (!m_colorRampShader)
		{
#if 0
			const char* vendorName = (const char*)glGetString(GL_VENDOR);
			ccLog::Print("[3D View %i] Graphic card manufacturer: %s",m_uniqueID,vendorName);
#endif
			//we will update global parameters
			ccGui::ParamStruct params = ccGui::Parameters();

			GLint maxBytes = 0;
			glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,&maxBytes);

			const GLint minRequiredBytes = ccColorRampShader::MinRequiredBytes();
			if (maxBytes < minRequiredBytes)
			{
				ccLog::Warning("[3D View %i] Not enough memory on shader side to use color ramp shader! (max=%i/%i bytes)",m_uniqueID,maxBytes,minRequiredBytes);
				params.colorScaleShaderSupported = false;
			}
			else
			{
				ccColorRampShader* colorRampShader = new ccColorRampShader();
				std::string shadersPath = ccGLWindow::getShadersPath();
				if (!colorRampShader->loadProgram(0,(shadersPath+"/ColorRamp/color_ramp.frag").c_str()))
				{
					ccLog::Warning("[3D View %i] Failed to load color ramp shader!",m_uniqueID);
					params.colorScaleShaderSupported = false;
					delete colorRampShader;
					colorRampShader = 0;
				}
				else
				{
					ccLog::Print("[3D View %i] Color ramp shader loaded successfully",m_uniqueID);
					m_colorRampShader = colorRampShader;
					params.colorScaleShaderSupported = true;
					params.colorScaleUseShader = false;

#if 0
					//if global parameter is not yet defined
					if (!ccGui::Parameters().isInPersistentSettings("colorScaleUseShader"))
					{
						bool shouldUseShader = true;
						if (!vendorName || QString(vendorName).toUpper().startsWith("ATI"))
						{
							ccLog::Warning("[3D View %i] Color ramp shader will remain disabled as it may not work on %s cards!\nYou can manually activate it in the display settings (at your own risk!)",m_uniqueID,vendorName);
							shouldUseShader = false;
						}
						params.colorScaleUseShader = shouldUseShader;
					}
#endif
				}
			}

			ccGui::Set(params);
		}
	}

	//transparency off by default
	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//no global ambiant
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ccColor::night);

	ccGLUtils::CatchGLError("ccGLWindow::initializeGL");

	m_initialized = true;

	ccLog::Print("[ccGLWindow] 3D view initialized");
}

static bool s_resizeGLInitSuccess=true;
void ccGLWindow::resizeGL(int w, int h)
{
	s_resizeGLInitSuccess = true;

	m_glWidth = w;
	m_glHeight = h;

	//DGM --> QGLWidget doc: 'There is no need to call makeCurrent() because this has already been done when this function is called."
	//makeCurrent();

	//update OpenGL viewport
	glViewport(0,0,w,h);

	invalidateViewport();
	invalidateVisualization();

	//filters
	if (m_fbo || m_alwaysUseFBO)
		s_resizeGLInitSuccess &= initFBO(m_glWidth,m_glHeight);
	if (m_activeGLFilter)
		s_resizeGLInitSuccess &= initGLFilter(m_glWidth,m_glHeight);

	//pivot symbol is dependent on the screen size!
	if (m_pivotGLList != GL_INVALID_LIST_ID)
	{
		glDeleteLists(m_pivotGLList,1);
		m_pivotGLList = GL_INVALID_LIST_ID;
	}

	char buf[256];
	sprintf(buf, "New size = %d * %d (px)", w, h);
	displayNewMessage(buf,
						ccGLWindow::LOWER_LEFT_MESSAGE,
						false,
						2,
						SCREEN_SIZE_MESSAGE);

	s_resizeGLInitSuccess &= !ccGLUtils::CatchGLError("ccGLWindow::resizeGL");
}

void ccGLWindow::paintGL()
{
	//context initialization
	CC_DRAW_CONTEXT context;
	getContext(context);

	bool doDraw3D = (!m_fbo || ((m_alwaysUseFBO && m_updateFBO) || m_activeGLFilter || m_captureMode));

	if (doDraw3D)
	{
		bool doDrawCross = (!m_captureMode && !m_params.perspectiveView && !(m_fbo && m_activeGLFilter) && ccGui::Parameters().displayCross);
		draw3D(context,doDrawCross,m_fbo);
		m_updateFBO = false;
	}

	/****************************************/
	/****  PASS: 2D/FOREGROUND/NO LIGHT  ****/
	/****************************************/
	context.flags = CC_DRAW_2D | CC_DRAW_FOREGROUND;
	if (m_interactionMode == TRANSFORM_ENTITY)		
		context.flags |= CC_VIRTUAL_TRANS_ENABLED;

	setStandardOrthoCenter();
	glDisable(GL_DEPTH_TEST);

	GLuint screenTex = 0;
	if (m_fbo)
	{
		if (m_activeGLFilter)
		{
			//we process GL filter
			GLuint depthTex = m_fbo->getDepthTexture();
			GLuint colorTex = m_fbo->getColorTexture(0);
			m_activeGLFilter->shade(depthTex, colorTex, (m_params.perspectiveView ? computePerspectiveZoom() : m_params.zoom)); //DGM FIXME

			ccGLUtils::CatchGLError("ccGLWindow::paintGL/glFilter shade");

			//if capture mode is ON: we only want to capture it, not to display it
			if (!m_captureMode)
				screenTex = m_activeGLFilter->getTexture();
		}
		else if (!m_captureMode)
		{
			//screenTex = m_fbo->getDepthTexture();
			screenTex = m_fbo->getColorTexture(0);
		}
	}

	//if any, we display texture fullscreen
	if (glIsTexture(screenTex))
	{
		ccGLUtils::DisplayTexture2D(screenTex,m_glWidth,m_glHeight);
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	//we draw 2D entities
	if (m_globalDBRoot)
		m_globalDBRoot->draw(context);
	if (m_winDBRoot)
		m_winDBRoot->draw(context);

	//current displayed scalar field color ramp (if any)
	ccRenderingTools::DrawColorRamp(context);

	//overlay entities
	if (!m_captureMode)
	{
		//default overlay color
		const unsigned char* textCol = ccGui::Parameters().textDefaultCol;

		//transparent border at the bottom of the screen
		if (m_activeGLFilter)
		{
			float w = (float)m_glWidth*0.5f;
			float h = (float)m_glHeight*0.5f;
			int borderWidth = 2*CC_DISPLAYED_TRIHEDRON_AXES_LENGTH+10;

			glPushAttrib(GL_COLOR_BUFFER_BIT);
			glEnable(GL_BLEND);

			glColor4f(1.0f,1.0f,0.0f,0.6f);
			glBegin(GL_QUADS);
			glVertex2f(-w,-h+(float)borderWidth);
			glVertex2f(w,-h+(float)borderWidth);
			glVertex2f(w,-h);
			glVertex2f(-w,-h);
			glEnd();

			glPopAttrib();

			textCol = ccColor::black;
			//Some versions of Qt seem to need glColorf instead of glColorub! (see https://bugreports.qt-project.org/browse/QTBUG-6217)
			//glColor3ubv(textCol);
			glColor3f((float)textCol[0]/(float)MAX_COLOR_COMP,(float)textCol[1]/(float)MAX_COLOR_COMP,(float)textCol[2]/(float)MAX_COLOR_COMP);
			renderText(10, m_glHeight-(float)borderWidth+16,std::string("[GL filter] ")+m_activeGLFilter->getName()); //we ignore the custom font size
		}

		//trihedron
		drawTrihedron();

		//scale (only in ortho mode)
		if (!m_params.perspectiveView)
		{
			//if fbo --> override color
			drawScale(m_fbo && m_activeGLFilter ? ccColor::black : textCol);
		}

		//current messages (if valid)
		if (!m_messagesToDisplay.empty())
		{
			int currentTime_sec = ccTimer::Sec();
			//ccLog::Print(QString("[paintGL] Current time: %1 s.").arg(currentTime_sec));

			//if fbo --> override color
			//Some versions of Qt seem to need glColorf instead of glColorub! (see https://bugreports.qt-project.org/browse/QTBUG-6217)
			//glColor3ubv(m_fbo && m_activeGLFilter ? ccColor::black : textCol);
			const unsigned char* col = (m_fbo && m_activeGLFilter ? ccColor::black : textCol);
			glColor3f((float)col[0]/(float)MAX_COLOR_COMP,(float)col[1]/(float)MAX_COLOR_COMP,(float)col[2]/(float)MAX_COLOR_COMP);

			int ll_currentHeight = m_glHeight-10; //lower left
			int uc_currentHeight = 10; //upper center

			std::list<MessageToDisplay>::iterator it = m_messagesToDisplay.begin();
			while (it != m_messagesToDisplay.end())
			{
				//no more valid? we delete the message
				if (it->messageValidity_sec < currentTime_sec)
				{
					it = m_messagesToDisplay.erase(it);
				}
				else
				{
					switch(it->position)
					{
					case LOWER_LEFT_MESSAGE:
						{
							renderText(10, ll_currentHeight, it->message);
							int messageHeight = 16;
							ll_currentHeight -= (messageHeight*5)/4; //add a 25% margin
						}
						break;
					case UPPER_CENTER_MESSAGE:
						{
							int width = toGB2312(it->message).length() * 8;
							renderText((m_glWidth-width)/2, uc_currentHeight+16, it->message);
							uc_currentHeight += (16*5)/4; //add a 25% margin
						}
						break;
					case SCREEN_CENTER_MESSAGE:
						{
							int width = toGB2312(it->message).length() * 8;
							//only one message supported in the screen center (for the moment ;)
							renderText((m_glWidth-width)/2, (m_glHeight-16)/2, it->message);
						}
						break;
					}
					++it;
				}
			}
		}

		if (m_hotZoneActivated)
		{
			//display parameters
			static const char c_psi_title[] = "Default point size";
			static const int c_psi_margin = 16;
			static const int c_psi_iconSize = 16;
			static const unsigned char c_psi_alphaChannel = 200;

			glPushAttrib(GL_COLOR_BUFFER_BIT);
			glEnable(GL_BLEND);

			//label
			glColor4f(0.52f,0.76f,0.15f,(float)c_psi_alphaChannel/(float)MAX_COLOR_COMP);
			renderText(c_psi_margin,(c_psi_margin+c_psi_iconSize/2)+(16/2)*2/3,c_psi_title); // --> 2/3 to compensate the effect of the upper case letter (P)

			//icons
			int halfW = m_glWidth/2;
			int halfH = m_glHeight/2;

			int xStart = c_psi_margin+8*strlen(c_psi_title)+c_psi_margin;
			int yStart = (c_psi_margin+c_psi_iconSize/2)+(16/2)*2/3;

			//"minus"
			renderText(xStart+4,yStart, "-");
			m_hotZoneMinusIconROI[0]=xStart;
			m_hotZoneMinusIconROI[1]=yStart-c_psi_iconSize;
			m_hotZoneMinusIconROI[2]=xStart+c_psi_iconSize;
			m_hotZoneMinusIconROI[3]=yStart;

			xStart += c_psi_iconSize;

			//separator
			glColor4ub(133,193,39,c_psi_alphaChannel);
			glBegin(GL_POINTS);
			glVertex2i(xStart+c_psi_margin/2,yStart+c_psi_iconSize/2);
			glEnd();
			xStart += c_psi_margin;

			//"plus"
			renderText(xStart+4,yStart, "+");
			m_hotZonePlusIconROI[0]=xStart;
			m_hotZonePlusIconROI[1]=m_hotZoneMinusIconROI[1];
			m_hotZonePlusIconROI[2]=xStart+c_psi_iconSize;
			m_hotZonePlusIconROI[3]=m_hotZoneMinusIconROI[3];
			xStart += c_psi_iconSize;

			glDisable(GL_BLEND);
			glPopAttrib();
		}
	}

	ccGLUtils::CatchGLError("ccGLWindow::paintGL");

	m_shouldBeRefreshed=false;
}

void ccGLWindow::draw3D(CC_DRAW_CONTEXT& context, bool doDrawCross, ccFrameBufferObject* fbo/*=0*/)
{
	makeCurrent();

	//if a FBO is activated
	if (fbo)
	{
		fbo->start();
		ccGLUtils::CatchGLError("ccGLWindow::paintGL/FBO start");
	}

	setStandardOrthoCenter();
	glDisable(GL_DEPTH_TEST);

	glPointSize(m_params.defaultPointSize);
	glLineWidth(m_params.defaultLineWidth);

	//gradient color background
	if (ccGui::Parameters().drawBackgroundGradient)
	{
		drawGradientBackground();
		//we clear background
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	else
	{
		const unsigned char* bkgCol = ccGui::Parameters().backgroundCol;
		glClearColor(	(float)bkgCol[0] / 255.0f,
						(float)bkgCol[1] / 255.0f,
						(float)bkgCol[2] / 255.0f,
						1.0f);

		//we clear background
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	/****************************************/
	/****  PASS: 2D/BACKGROUND/NO LIGHT  ****/
	/****************************************/
	/*context.flags = CC_DRAW_2D;
	if (m_interactionMode == TRANSFORM_ENTITY)		
		context.flags |= CC_VIRTUAL_TRANS_ENABLED;

	//we draw 2D entities
	if (m_globalDBRoot)
		m_globalDBRoot->draw(context);
	if (m_winDBRoot)
		m_winDBRoot->draw(context);
	//*/

	/****************************************/
	/****  PASS: 3D/BACKGROUND/NO LIGHT  ****/
	/****************************************/
	context.flags = CC_DRAW_3D | CC_DRAW_FOREGROUND;
	if (m_interactionMode == TRANSFORM_ENTITY)		
		context.flags |= CC_VIRTUAL_TRANS_ENABLED;

	glEnable(GL_DEPTH_TEST);

	if (doDrawCross)
		drawCross();

	/****************************************/
	/****    PASS: 3D/FOREGROUND/LIGHT   ****/
	/****************************************/
	if (m_customLightEnabled || m_sunLightEnabled)
		context.flags |= CC_LIGHT_ENABLED;
	if (m_lodActivated)
		context.flags |= CC_LOD_ACTIVATED;

	//we enable absolute sun light (if activated)
	if (m_sunLightEnabled)
		glEnableSunLight();

	//we setup projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(getProjectionMatd());

	//then, the modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(getModelViewMatd());

	//we enable relative custom light (if activated)
	if (m_customLightEnabled)
	{
		glEnableCustomLight();
		if (!m_captureMode/* && !m_params.perspectiveView*/)
			//we display it as a litle 3D star
			drawCustomLight();
	}

	//we activate the current shader (if any)
	if (m_activeShader)
		m_activeShader->start();
	
	//color ramp shader for fast dynamic color ramp lookup-up
	if (m_colorRampShader && ccGui::Parameters().colorScaleUseShader)
		context.colorRampShader = m_colorRampShader;

	//we draw 3D entities
	if (m_globalDBRoot)
	{
		m_globalDBRoot->draw(context);
		if (m_globalDBRoot->getChildrenNumber())
		{
			//draw pivot
			drawPivot();
		}
	}
	if (m_winDBRoot)
		m_winDBRoot->draw(context);

	//for connected items
	/*emit*/if (drawing3D) drawing3D();

	//we disable shader (if any)
	if (m_activeShader)
		m_activeShader->stop();

	//we disable lights
	if (m_sunLightEnabled)
		glDisableSunLight();
	if (m_customLightEnabled)
		glDisableCustomLight();

	//we disable fbo
	if (fbo)
	{
		fbo->stop();
		ccGLUtils::CatchGLError("ccGLWindow::paintGL/FBO stop");
	}
}

bool ccGLWindow::objectPerspectiveEnabled() const
{
	return m_params.perspectiveView && m_params.objectCenteredView;
}

bool ccGLWindow::viewerPerspectiveEnabled() const
{
	return m_params.perspectiveView && !m_params.objectCenteredView;
}

bool ccGLWindow::getPerspectiveState(bool& objectCentered) const
{
	objectCentered = m_params.objectCenteredView;
	return m_params.perspectiveView;
}

void ccGLWindow::setUnclosable(bool state)
{
	m_unclosable = state;
}

ccHObject* ccGLWindow::getOwnDB()
{
	return m_winDBRoot;
}

void ccGLWindow::addToOwnDB(ccHObject* obj2D)
{
	assert(obj2D);

	if (m_winDBRoot)
	{
		m_winDBRoot->addChild(obj2D,false);
		obj2D->setDisplay(this);
	}
	else
	{
		ccLog::Error("[ccGLWindow::addToOwnDB] Window has no DB!");
	}
}

void ccGLWindow::removeFromOwnDB(ccHObject* obj2D)
{
	if (m_winDBRoot)
		m_winDBRoot->removeChild(obj2D);
}

void ccGLWindow::zoomGlobal()
{
	updateConstellationCenterAndZoom();
}

void ccGLWindow::updateConstellationCenterAndZoom(const ccBBox* aBox/*=0*/)
{
	setZoom(1.0);

	ccBBox zoomedBox;

	//the user has provided a valid bouding box
	if (aBox)
	{
		zoomedBox = (*aBox);
	}
	else if (m_globalDBRoot) //otherwise we'll take the default one (if possible)
	{
		zoomedBox = m_globalDBRoot->getBB(true, true, this);
	}

	if (!zoomedBox.isValid())
		return;

	//we get the bounding-box diagonal length
	float bbDiag = zoomedBox.getDiagNorm();

	if (bbDiag < ZERO_TOLERANCE)
	{
		ccLog::Warning("[ccGLWindow] Entity/DB has a null bounding-box! Can't zoom in..."); 
		return;
	}

	//we compute the pixel size (in world coordinates)
	{
		int minScreenSize = std::min(m_glWidth,m_glHeight);
		m_params.pixelSize = (minScreenSize > 0 ? bbDiag / static_cast<float>(minScreenSize) : 1.0f);
	}

	//we set the pivot point on the box center
	CCVector3 P = zoomedBox.getCenter();
	setPivotPoint(P);
	
	if (m_params.perspectiveView)
	{
		//we must go backward so as to see the object!
		assert(m_params.fov > ZERO_TOLERANCE);
		float d = bbDiag / tan(m_params.fov*CC_DEG_TO_RAD);
		setCameraPos(P - getCurrentViewDir() * d);
	}
	else
	{
		setCameraPos(P); //camera is on the pivot in ortho mode
	}

	invalidateViewport();
	invalidateVisualization();

	redraw();
}

bool ccGLWindow::areShadersEnabled() const
{
	return m_shadersEnabled;
}

bool ccGLWindow::areGLFiltersEnabled() const
{
	return m_glFiltersEnabled;
}

void ccGLWindow::setShader(ccShader* _shader)
{
	if (!m_shadersEnabled)
	{
		ccLog::Warning("[ccGLWindow::setShader] Shader ignored (not supported)");
		return;
	}

	if (m_activeShader)
		delete m_activeShader;
	m_activeShader = _shader;

	redraw();
}

void ccGLWindow::setGlFilter(ccGlFilter* filter)
{
	if (!m_glFiltersEnabled)
	{
		ccLog::Warning("[ccGLWindow::setGlFilter] GL filter ignored (not supported)");
		return;
	}

	removeGLFilter();

	if (filter)
	{
		if (!m_fbo)
		{
			if (!initFBO(m_glWidth,m_glHeight))
			{
				redraw();
				return;
			}
		}

		m_activeGLFilter = filter;
		initGLFilter(m_glWidth,m_glHeight);
	}

	if (!m_activeGLFilter && m_fbo && !m_alwaysUseFBO)
		removeFBO();

	redraw();
}

void ccGLWindow::setZoom(float value)
{
	if (value < CC_GL_MIN_ZOOM_RATIO)
		value = CC_GL_MIN_ZOOM_RATIO;
	else if (value > CC_GL_MAX_ZOOM_RATIO)
		value = CC_GL_MAX_ZOOM_RATIO;

	if (m_params.zoom != value)
	{
		m_params.zoom = value;
		invalidateViewport();
		invalidateVisualization();
	}
}

void ccGLWindow::setCameraPos(const CCVector3& P)
{
	m_params.cameraCenter = P;
	/*emit*/if (cameraPosChanged) cameraPosChanged(m_params.cameraCenter);

	invalidateViewport();
	invalidateVisualization();
}

void ccGLWindow::moveCamera(float dx, float dy, float dz)
{
	if (dx != 0 || dy != 0) //camera movement? (dz doesn't count as ot only corresponds to a zoom)
	{
		//feedback for echo mode
		/*emit*/if (cameraDisplaced) cameraDisplaced(dx,dy);
	}

	//curent X, Y and Z viewing directions
	//correspond to the 'model view' matrix
	//lines.
	CCVector3 V(dx,dy,dz);
	if (!m_params.objectCenteredView)
		m_params.viewMat.transposed().applyRotation(V);

	setCameraPos(m_params.cameraCenter + V);
}

void ccGLWindow::setPivotPoint(const CCVector3& P)
{
	m_params.pivotPoint = P;
	/*emit*/if (pivotPointChanged) pivotPointChanged(m_params.pivotPoint);

	invalidateViewport();
	invalidateVisualization();
}

void ccGLWindow::setSceneDB(ccHObject* root)
{
	m_globalDBRoot = root;
	zoomGlobal();
}

ccHObject* ccGLWindow::getSceneDB()
{
	return m_globalDBRoot;
}

void ccGLWindow::drawGradientBackground()
{
	int w = m_glWidth/2+1;
	int h = m_glHeight/2+1;

	const unsigned char* bkgCol = ccGui::Parameters().backgroundCol;
	const unsigned char* forCol = ccGui::Parameters().textDefaultCol;

	//Gradient "texture" drawing
	glBegin(GL_QUADS);
	//we the user-defined background color for gradient start
	glColor3ubv(bkgCol);
	glVertex2f(-w,h);
	glVertex2f(w,h);
	//and the inverse of points color for gradient end
	glColor3ub(255-forCol[0],255-forCol[1],255-forCol[2]);
	glVertex2f(w,-h);
	glVertex2f(-w,-h);
	glEnd();
}

void ccGLWindow::drawCross()
{
	//cross OpenGL drawing
	glColor3ubv(ccColor::lightGrey);
	glBegin(GL_LINES);
	glVertex3f(0.0,-CC_DISPLAYED_CENTER_CROSS_LENGTH,0.0);
	glVertex3f(0.0,CC_DISPLAYED_CENTER_CROSS_LENGTH,0.0);
	glVertex3f(-CC_DISPLAYED_CENTER_CROSS_LENGTH,0.0,0.0);
	glVertex3f(CC_DISPLAYED_CENTER_CROSS_LENGTH,0.0,0.0);
	glEnd();
}

void ccGLWindow::drawScale(const colorType color[] /*= white*/)
{
	assert(!m_params.perspectiveView); //a scale is only valid in ortho. mode!

	float scaleMaxW = float(m_glWidth)*0.25; //25% de l'ecran
	if (m_params.zoom < CC_GL_MIN_ZOOM_RATIO)
	{
		assert(false);
		return;
	}

	//we first compute the width equivalent to 25% of horizontal screen width
	//(this is why it's only valid in orthographic mode !)
	float equivalentWidth = scaleMaxW * m_params.pixelSize / m_params.zoom;

	//we then compute the scale granularity (to avoid width values with a lot of decimals)
	int k = int(floor(log((float)equivalentWidth)/log((float)10.0)));
	float granularity =pow((float)10.0,(float)k)*0.5;

	//we choose the value closest to equivalentWidth with the right granularity
	equivalentWidth = floor(std::max(equivalentWidth/granularity,1.0f))*granularity;

	//we deduce the scale drawing width
	float scaleW_pix = equivalentWidth/m_params.pixelSize * m_params.zoom;
	float dW = 2*CC_DISPLAYED_TRIHEDRON_AXES_LENGTH+20.0;
	float dH = std::max<float>((float)12/*fm.height()*/*1.25,CC_DISPLAYED_TRIHEDRON_AXES_LENGTH+5.0);
	float w = float(m_glWidth)*0.5-dW;
	float h = float(m_glHeight)*0.5-dH;

	//scale OpenGL drawing
	glColor3ubv(color);
	glBegin(GL_LINES);
	glVertex3f(w-scaleW_pix,-h,0.0);
	glVertex3f(w,-h,0.0);
	glVertex3f(w-scaleW_pix,-h-3,0.0);
	glVertex3f(w-scaleW_pix,-h+3,0.0);
	glVertex3f(w,-h+3,0.0);
	glVertex3f(w,-h-3,0.0);
	glEnd();

	std::string text = std::to_string(equivalentWidth);
	//Some versions of Qt seem to need glColorf instead of glColorub! (see https://bugreports.qt-project.org/browse/QTBUG-6217)
	glColor3f((float)color[0]/(float)MAX_COLOR_COMP,(float)color[1]/(float)MAX_COLOR_COMP,(float)color[2]/(float)MAX_COLOR_COMP);
	renderText(m_glWidth-int(scaleW_pix*0.5+dW)-text.length()*4, m_glHeight-dH/2+16/3, text);
}

void ccGLWindow::drawTrihedron()
{
	float w = static_cast<float>(m_glWidth)*0.5-CC_DISPLAYED_TRIHEDRON_AXES_LENGTH-10.0;
	float h = static_cast<float>(m_glHeight)*0.5-CC_DISPLAYED_TRIHEDRON_AXES_LENGTH-5.0;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(w, -h, 0.0);
	glMultMatrixf(m_params.viewMat.data());

	if (m_trihedronGLList == GL_INVALID_LIST_ID)
	{
		m_trihedronGLList = glGenLists(1);
		glNewList(m_trihedronGLList, GL_COMPILE);

		glPushAttrib(GL_LINE_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_DEPTH_TEST);

		//trihedron OpenGL drawing
		glBegin(GL_LINES);
		glColor3f(1.0f,0.0f,0.0f);
		glVertex3f(0.0f,0.0f,0.0f);
		glVertex3f(CC_DISPLAYED_TRIHEDRON_AXES_LENGTH,0.0f,0.0f);
		glColor3f(0.0f,1.0f,0.0f);
		glVertex3f(0.0f,0.0f,0.0f);
		glVertex3f(0.0f,CC_DISPLAYED_TRIHEDRON_AXES_LENGTH,0.0f);
		glColor3f(0.0f,0.7f,1.0f);
		glVertex3f(0.0f,0.0f,0.0f);
		glVertex3f(0.0f,0.0f,CC_DISPLAYED_TRIHEDRON_AXES_LENGTH);
		glEnd();

		glPopAttrib();

		glEndList();
	}
	glCallList(m_trihedronGLList);

	glPopMatrix();
}

void ccGLWindow::invalidateViewport()
{
	m_validProjectionMatrix=false;
	m_updateFBO = true;
}

void ccGLWindow::recalcProjectionMatrix()
{
	makeCurrent();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float bbHalfDiag = 1.0f;
	CCVector3 bbCenter(0.0f);

	//compute center of viewed objects constellation
	if (m_globalDBRoot)
	{
		//get whole bounding-box
		ccBBox box = m_globalDBRoot->getBB(true, true, this);
		if (box.isValid())
		{
			//incorporate window own db
			if (m_winDBRoot)
			{
				ccBBox ownBox = m_winDBRoot->getBB(true, true, this);
				if (ownBox.isValid())
				{
					box.add(ownBox.minCorner());
					box.add(ownBox.maxCorner());
				}
			}
			//get bbox center
			bbCenter = box.getCenter();
			//get half bbox diagonal length
			bbHalfDiag = box.getDiagNorm()*0.5;
		}
	}

	//virtual pivot point (i.e. to handle viewer-based mode smoothly)
	CCVector3 pivotPoint = (m_params.objectCenteredView ? m_params.pivotPoint : bbCenter);

	//distance between camera and pivot point
	float CP = (m_params.cameraCenter-pivotPoint).norm();
	//distance between pivot point and DB farthest point
	float MP = (bbCenter-pivotPoint).norm() + bbHalfDiag;

	//pivot symbol shoud always be (potentially) visible in object-based mode
	if (m_pivotSymbolShown && m_params.objectCenteredView && m_pivotVisibility != PIVOT_HIDE)
	//if (m_params.objectCenteredView)
	{
		float pivotActualRadius = (CC_DISPLAYED_PIVOT_RADIUS_PERCENT) * (float)std::min(m_glWidth,m_glHeight) * 0.5f;
		float pivotSymbolScale = pivotActualRadius * computeActualPixelSize();
		MP = std::max<float>(MP,pivotSymbolScale);
	}
	MP *= 1.01f; //for round-off issues
	
	if (m_customLightEnabled)
	{
		//distance from custom light to pivot point
		float d = CCVector3::vdistance(pivotPoint.u,m_customLightPos);
		MP = std::max<float>(MP,d);
	}

	if (m_params.perspectiveView)
	{
		//we deduce zNear et zFar
		//DGM: by default we clip zNear just after 0 (not too close,
		//otherwise it can cause a very strange behavior when looking
		//at objects with large coordinates)
		double zNear = static_cast<double>(MP)*1e-3;
		if (m_params.objectCenteredView)
			zNear = std::max<double>(CP-MP,zNear);

		double zFar = std::max<double>(CP+MP,1.0);

		//and aspect ratio
		double ar = static_cast<double>(m_glWidth)/static_cast<double>(m_glHeight);

		gluPerspective(m_params.fov,ar,zNear,zFar);
	}
	else
	{
		//max distance (camera to 'farthest' point)
		float maxDist = CP + MP;

		float maxDist_pix = maxDist / m_params.pixelSize * m_params.zoom;
		maxDist_pix = std::max<float>(maxDist_pix,1.0f);

		float halfW = static_cast<float>(m_glWidth)*0.5f;
		float halfH = static_cast<float>(m_glHeight)*0.5f;

		glOrtho(-halfW,halfW,-halfH,halfH,-maxDist_pix,maxDist_pix);
	}

	//we save projection matrix
	//glGetFloatv(GL_PROJECTION_MATRIX, m_projMat.data());
	glGetDoublev(GL_PROJECTION_MATRIX, m_projMatd);

	m_validProjectionMatrix = true;
}

void ccGLWindow::invalidateVisualization()
{
	m_validModelviewMatrix=false;
	m_updateFBO = true;
}

void ccGLWindow::recalcModelViewMatrix()
{
	makeCurrent();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (m_params.perspectiveView) //perspective mode
	{
		//for proper aspect ratio handling
		float ar = (m_glHeight != 0 ? float(m_glWidth)/(float(m_glHeight)*m_params.aspectRatio) : 0.0f);
		if (ar<1.0)
			glScalef(ar,ar,1.0);
	}
	else //ortho. mode
	{
		//apply zoom
		float totalZoom = m_params.zoom / m_params.pixelSize;
		glScalef(totalZoom,totalZoom,totalZoom);
	}

	//apply current camera parameters (see trunk/doc/rendering_pipeline.doc)
	if (m_params.objectCenteredView)
	{
		//place origin on camera center
		glTranslatef(-m_params.cameraCenter.x, -m_params.cameraCenter.y, -m_params.cameraCenter.z);

		//go back to initial origin
		glTranslatef(m_params.pivotPoint.x, m_params.pivotPoint.y, m_params.pivotPoint.z);

		//rotation (viewMat is simply a rotation matrix around the pivot here!)
		glMultMatrixf(m_params.viewMat.data());

		//place origin on pivot point
		glTranslatef(-m_params.pivotPoint.x, -m_params.pivotPoint.y, -m_params.pivotPoint.z);
	}
	else
	{
		//rotation (viewMat is the rotation around the camera center here - no pivot)
		glMultMatrixf(m_params.viewMat.data());

		//place origin on camera center
		glTranslatef(-m_params.cameraCenter.x, -m_params.cameraCenter.y, -m_params.cameraCenter.z);
	}		

	//we save visualization matrix
	//glGetFloatv(GL_MODELVIEW_MATRIX, m_viewMat.data());
	glGetDoublev(GL_MODELVIEW_MATRIX, m_viewMatd);

	m_validModelviewMatrix=true;
}

const ccGLMatrix& ccGLWindow::getBaseViewMat()
{
	return m_params.viewMat;
}

const void ccGLWindow::setBaseViewMat(ccGLMatrix& mat)
{
	m_params.viewMat = mat;

	invalidateVisualization();

	//we /*emit*/ the 'baseViewMatChanged' signal
	/*emit*/ if (baseViewMatChanged) baseViewMatChanged(m_params.viewMat);
}

const double* ccGLWindow::getModelViewMatd()
{
	if (!m_validModelviewMatrix)
		recalcModelViewMatrix();

	return m_viewMatd;
}

const double* ccGLWindow::getProjectionMatd()
{
	if (!m_validProjectionMatrix)
		recalcProjectionMatrix();

	return m_projMatd;
}

void ccGLWindow::setStandardOrthoCenter()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float halfW = float(m_glWidth)*0.5;
	float halfH = float(m_glHeight)*0.5;
	float maxS = std::max(halfW,halfH);
	glOrtho(-halfW,halfW,-halfH,halfH,-maxS,maxS);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void ccGLWindow::setStandardOrthoCorner()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,0,float(m_glWidth),float(m_glHeight),0,1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void ccGLWindow::getContext(CC_DRAW_CONTEXT& context)
{
	//display size
	context.glW = m_glWidth;
	context.glH = m_glHeight;
	context._win = this;
	context.flags = 0;

	const ccGui::ParamStruct& guiParams = ccGui::Parameters();

	//decimation options
	context.decimateCloudOnMove = guiParams.decimateCloudOnMove;
	context.decimateMeshOnMove = guiParams.decimateMeshOnMove;

	//scalar field colorbar
	context.sfColorScaleToDisplay = 0;

	//point picking
	float pixSize = computeActualPixelSize();
	context.pickedPointsRadius = (float)guiParams.pickedPointsSize * pixSize ;
	context.pickedPointsTextShift = 5.0 * pixSize; //5 pixels shift

	//text display
	context.dispNumberPrecision = guiParams.displayedNumPrecision;
	context.labelsTransparency = guiParams.labelsTransparency;

	//default materials
	context.defaultMat.name = "default";
	memcpy(context.defaultMat.diffuseFront,guiParams.meshFrontDiff,sizeof(float)*4);
	memcpy(context.defaultMat.diffuseBack,guiParams.meshBackDiff,sizeof(float)*4);
	memcpy(context.defaultMat.ambient,ccColor::bright,sizeof(float)*4);
	memcpy(context.defaultMat.specular,guiParams.meshSpecular,sizeof(float)*4);
	memcpy(context.defaultMat.emission,ccColor::night,sizeof(float)*4);
	context.defaultMat.shininessFront = 30;
	context.defaultMat.shininessBack = 50;
	//default colors
	memcpy(context.pointsDefaultCol,guiParams.pointsDefaultCol,sizeof(unsigned char)*3);
	memcpy(context.textDefaultCol,guiParams.textDefaultCol,sizeof(unsigned char)*3);
	memcpy(context.labelDefaultCol,guiParams.labelCol,sizeof(unsigned char)*3);
	memcpy(context.bbDefaultCol,guiParams.bbDefaultCol,sizeof(unsigned char)*3);
}

void ccGLWindow::toBeRefreshed()
{
	m_shouldBeRefreshed = true;

	invalidateViewport();
}

void ccGLWindow::refresh()
{
	if (m_shouldBeRefreshed /*&& isVisible()*/)
		redraw();
}

void ccGLWindow::redraw()
{
	m_updateFBO = true;
	updateGL();
}


CCVector3 ccGLWindow::getCurrentViewDir() const
{
	if (m_params.objectCenteredView)
		return CCVector3(0.0f,0.0f,-1.0f);

	//otherwise view direction is (the opposite of) the 3rd line of the current view matrix
	const float* M = m_params.viewMat.data();
	CCVector3 axis(-M[2],-M[6],-M[10]);
	axis.normalize();

	return axis;
}

CCVector3 ccGLWindow::getCurrentUpDir() const
{
	//if (m_params.objectCenteredView)
	//	return CCVector3(0.0f,1.0f,0.0f);

	//otherwise up direction is the 2nd line of the current view matrix
	const float* M = m_params.viewMat.data();
	CCVector3 axis(M[1],M[5],M[9]);
	axis.normalize();

	return axis;
}

void ccGLWindow::setInteractionMode(INTERACTION_MODE mode)
{
	m_interactionMode = mode;
}

void ccGLWindow::setPickingMode(PICKING_MODE mode/*=DEFAULT_PICKING*/)
{
	switch(mode)
	{
	case DEFAULT_PICKING:
		mode = ENTITY_PICKING;
	case NO_PICKING:
	case ENTITY_PICKING:
	  //setCursor(QCursor(Qt::ArrowCursor));
		break;
	case TRIANGLE_PICKING:
	case POINT_PICKING:
	  //setCursor(QCursor(Qt::PointingHandCursor));
		break;
	default:
	   break;
	}

	m_pickingMode = mode;
}

void ccGLWindow::enableEmbeddedIcons(bool state)
{
	m_embeddedIconsEnabled	= state;
	m_hotZoneActivated		= false;
	//setMouseTracking(state);
}

CCVector3 ccGLWindow::convertMousePositionToOrientation(int x, int y)
{
	PointCoordinateType xc = static_cast<PointCoordinateType>(width()/2);
	PointCoordinateType yc = static_cast<PointCoordinateType>(height()/2);

	GLdouble xp,yp;
	if (m_params.objectCenteredView)
	{
		GLdouble zp;
		
		//project the current pivot point on screen
		int VP[4];
		getViewportArray(VP);
		gluProject(m_params.pivotPoint.x,m_params.pivotPoint.y,m_params.pivotPoint.z,getModelViewMatd(),getProjectionMatd(),VP,&xp,&yp,&zp);

		//we set the virtual rotation pivot closer to the actual one (but we always stay in the central part of the screen!)
		xp = std::min<GLdouble>(xp,3*width()/4);
		xp = std::max<GLdouble>(xp,  width()/4);

		yp = std::min<GLdouble>(yp,3*height()/4);
		yp = std::max<GLdouble>(yp,  height()/4);
	}
	else
	{
		xp = static_cast<GLdouble>(xc);
		yp = static_cast<GLdouble>(yc);
	}

	//invert y
	y = height()-1 - y;

	//CCVector3 v(float(2 * std::max<int>(std::min<int>(x,width()-1),-(width()-1)) - width()) / (float)width(),
	//			float(height() - 2 * std::max<int>(std::min<int>(y,height()-1),-(height()-1))) / (float)height(),
	//			0.0f);
	CCVector3 v((PointCoordinateType)x - (PointCoordinateType)xp,
				(PointCoordinateType)y - (PointCoordinateType)yp,
				0);

	v.x = std::max<PointCoordinateType>(std::min<PointCoordinateType>(v.x/xc,1.0),-1.0);
	v.y = std::max<PointCoordinateType>(std::min<PointCoordinateType>(v.y/yc,1.0),-1.0);

	//square 'radius'
	PointCoordinateType d2 = v.x*v.x + v.y*v.y;

	//projection on the unit sphere
	if (d2 > 1.0f)
	{
		PointCoordinateType d = sqrt(d2);
		v.x /= d;
		v.y /= d;
	}
	else
	{
		v.z = sqrt((PointCoordinateType)1.0 - d2);
	}

	return v;
}

void ccGLWindow::updateActiveItemsList(int x, int y, bool extendToSelectedLabels/*=false*/)
{
	m_activeItems.clear();

	if (!m_globalDBRoot && !m_winDBRoot)
		return;

	if (m_interactionMode == TRANSFORM_ENTITY) //labels are ignored in 'Interactive Transformation' mode
		return;

	int subID=-1;
	int itemID = startPicking(FAST_PICKING,x,y,2,2,&subID);
	if (itemID<1)
		return;

	//items can be in local or global DB
	ccHObject* pickedObj = m_globalDBRoot->find(itemID);
	if (!pickedObj && m_winDBRoot)
		pickedObj = m_winDBRoot->find(itemID);
	if (pickedObj)
	{
		if (pickedObj->isA(CC_2D_LABEL))
		{
			cc2DLabel* label = static_cast<cc2DLabel*>(pickedObj);
			if (!label->isSelected() || !extendToSelectedLabels)
			{
				//select it?
				//emit entitySelectionChanged(label->getUniqueID());
				//QApplication::processEvents();
				m_activeItems.push_back(label);
				return;
			}
			else
			{
				//we get the other selected labels as well!
				ccHObject::Container labels;
				if (m_globalDBRoot)
					m_globalDBRoot->filterChildren(labels,true,CC_2D_LABEL);
				if (m_winDBRoot)
					m_winDBRoot->filterChildren(labels,true,CC_2D_LABEL);

				for (ccHObject::Container::iterator it=labels.begin(); it!=labels.end(); ++it)
					if ((*it)->isVisible())
					{
						cc2DLabel* label = static_cast<cc2DLabel*>(*it);
						if (label->isSelected())
						{
							m_activeItems.push_back(label);
						}
					}
			}
		}
		else if (pickedObj->isA(CC_CLIPPING_BOX))
		{
			ccClipBox* cbox = static_cast<ccClipBox*>(pickedObj);
			cbox->setActiveComponent(subID);
			cbox->setClickedPoint(x,y,width(),height(),m_params.viewMat);

			m_activeItems.push_back(cbox);
		}
	}
}

void ccGLWindow::handleEvent(SDL_Event *event)
{
	switch (event->type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		keyboardEvent(&event->key);
		break;

	case SDL_MOUSEBUTTONDOWN:
		mousePressEvent(&event->button);
		break;

	case SDL_MOUSEBUTTONUP:
		mouseReleaseEvent(&event->button);
		break;

	case SDL_MOUSEMOTION:
		mouseMoveEvent(&event->motion);
		break;

	case SDL_MOUSEWHEEL:
		wheelEvent(&event->wheel);
		break;

	default:
		break;
	}
}

void ccGLWindow::keyboardEvent(SDL_KeyboardEvent *event)
{
    // TODO
}

void ccGLWindow::mousePressEvent(SDL_MouseButtonEvent *event)
{
	m_cursorMoved = false;

	if (event->button == SDL_BUTTON_RIGHT)
	{
		if (m_interactionMode != SEGMENT_ENTITY && m_interactionMode != DRAW_CUBE) //mouse movement = panning (2D translation)
		{
			m_lastMousePos.x = event->x;
			m_lastMousePos.y = event->y;
			m_lodActivated = true;
		}

		/*emit*/ if (rightButtonClicked) rightButtonClicked(event->x-width()/2,height()/2-event->y);
	}
	else if (event->button == SDL_BUTTON_LEFT)
	{
		if (m_interactionMode != SEGMENT_ENTITY && m_interactionMode != DRAW_CUBE) //mouse movement = rotation
		{
			m_lastClickTime_ticks = ccTimer::Msec();

			m_lastMouseOrientation = convertMousePositionToOrientation(event->x, event->y);
			m_lastMousePos.x = event->x;
			m_lastMousePos.y = event->y;
			m_lodActivated = true;
			//let's check if the mouse is on a selected item first!
			const Uint8 *state = SDL_GetKeyboardState(NULL);
			if (!state[SDL_SCANCODE_LSHIFT] && !state[SDL_SCANCODE_RSHIFT] &&
			    !state[SDL_SCANCODE_RALT] && !state[SDL_SCANCODE_LALT])
				updateActiveItemsList(event->x, event->y, true);
		}

		/*emit*/ if (leftButtonClicked) leftButtonClicked(event->x-width()/2,height()/2-event->y);
	}
	else
	{
	}
}

void ccGLWindow::mouseMoveEvent(SDL_MouseMotionEvent *event)
{
	if (m_interactionMode == SEGMENT_ENTITY)
	{
		if (!(event->state & (SDL_BUTTON_LMASK|SDL_BUTTON_RMASK|SDL_BUTTON_MMASK)) || m_alwaysUseFBO) //fast!
			/*emit*/ if (mouseMoved) mouseMoved(event->x-width()/2,height()/2-event->y,event->state);
		return;
	}
	else if (m_interactionMode == DRAW_CUBE)
	{
		/*emit*/ if (mouseMoved) mouseMoved(event->x-width()/2,height()/2-event->y,event->state);
		return;
	}

	const int x = event->x;
	const int y = event->y;

	//no button pressed
	if ((event->state & (SDL_BUTTON_LMASK|SDL_BUTTON_RMASK|SDL_BUTTON_MMASK)) == 0)
	{
		if (m_embeddedIconsEnabled)
		{
			if (x < CC_HOT_ZONE_WIDTH && y < CC_HOT_ZONE_HEIGHT)
			{
				if (!m_hotZoneActivated)
				{
					m_hotZoneActivated = true;
					updateGL();
				}
			}
			else
			{
				if (m_hotZoneActivated)
				{
					m_hotZoneActivated = false;
					updateGL();
				}
			}
			//event->accept();
		}
		else
		{
			//event->ignore();
		}

		//don't need to process any further
		return;
	}

	int dx = x - m_lastMousePos.x;
	int dy = y - m_lastMousePos.y;

	if (event->state & SDL_BUTTON_RMASK)
	{
		//displacement vector (in "3D")
		float pixSize = computeActualPixelSize();
		CCVector3 u(static_cast<float>(dx)*pixSize, -static_cast<float>(dy)*pixSize, 0);

		const Uint8 *state = SDL_GetKeyboardState(NULL);
		bool entityMovingMode = (m_interactionMode == TRANSFORM_ENTITY) || ((state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL]) && m_customLightEnabled);
		if (entityMovingMode)
		{
			//apply inverse view matrix
			m_params.viewMat.transposed().applyRotation(u);

			if (m_interactionMode == TRANSFORM_ENTITY)
			{
				/*emit*/ if (translation) translation(u);
			}
			else if (m_customLightEnabled)
			{
				//update custom light position
				m_customLightPos[0] += u.x;
				m_customLightPos[1] += u.y;
				m_customLightPos[2] += u.z;
				invalidateViewport();
			}
		}
		else //camera moving mode
		{
			if (m_params.objectCenteredView)
			{
				//inverse displacement in object-based mode
				u *= -1.0;
			}			
			moveCamera(u.x,u.y,u.z);
		}
	}
	else if (event->state & SDL_BUTTON_LMASK) //rotation
	{
		//specific case: move active item(s)
		if (!m_activeItems.empty())
		{
			//displacement vector (in "3D")
			float pixSize = computeActualPixelSize();
			CCVector3 u(static_cast<float>(dx)*pixSize, -static_cast<float>(dy)*pixSize, 0);
			m_params.viewMat.transposed().applyRotation(u);

			for (std::list<ccInteractor*>::iterator it=m_activeItems.begin(); it!=m_activeItems.end(); ++it)
			{
				if ((*it)->move2D(x,y,dx,dy,width(),height()) || (*it)->move3D(u))
				{
					invalidateViewport();
					//m_updateFBO = true; //already done by invalidateViewport
				}
			}
		}
		else
		{
			//specific case: rectangular polyline drawing (for rectangular area selection mode)
			const Uint8 *state = SDL_GetKeyboardState(NULL);
			if (	m_allowRectangularEntityPicking
				&& (m_pickingMode == ENTITY_PICKING || m_pickingMode == ENTITY_RECT_PICKING)
				&& (m_rectPickingPoly || (state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT])))
			{
				//first time: initialization of the rectangle
				if (!m_rectPickingPoly)
				{
					ccPointCloud* vertices = new ccPointCloud("rect.vertices");
					m_rectPickingPoly = new ccPolyline(vertices);
					if (vertices->reserve(4) && m_rectPickingPoly->addPointIndex(0,4))
					{
						m_rectPickingPoly->setForeground(true);
						m_rectPickingPoly->setColor(ccColor::green);
						m_rectPickingPoly->showColors(true);
						m_rectPickingPoly->set2DMode(true);
						m_rectPickingPoly->setDisplay(this);
						m_rectPickingPoly->setVisible(true);
						CCVector3 A(m_lastMousePos.x-width()/2, height()/2-m_lastMousePos.y, 0);
						//we add 4 times the same point (just to fill the cloud!)
						vertices->addPoint(A);
						vertices->addPoint(A);
						vertices->addPoint(A);
						vertices->addPoint(A);
						m_rectPickingPoly->setClosingState(true);
						addToOwnDB(m_rectPickingPoly);
					}
					else
					{
						ccLog::Warning("[ccGLWindow] Failed to create seleciton polyline! Not enough memory!");
						delete m_rectPickingPoly;
						m_rectPickingPoly=0;
						delete vertices;
						vertices=0;
					}
				}

				if (m_rectPickingPoly)
				{
					CCLib::GenericIndexedCloudPersist* vertices = m_rectPickingPoly->getAssociatedCloud();
					assert(vertices);
					CCVector3* B = const_cast<CCVector3*>(vertices->getPointPersistentPtr(1));
					CCVector3* C = const_cast<CCVector3*>(vertices->getPointPersistentPtr(2));
					CCVector3* D = const_cast<CCVector3*>(vertices->getPointPersistentPtr(3));
					B->x = C->x = event->x - width()/2;
					C->y = D->y = height()/2 - event->y;
				}
			}
			else //standard rotation around the current pivot
			{
				m_currentMouseOrientation = convertMousePositionToOrientation(x, y);

				ccGLMatrix rotMat = ccGLUtils::GenerateGLRotationMatrixFromVectors(m_lastMouseOrientation.u,m_currentMouseOrientation.u);
				m_lastMouseOrientation = m_currentMouseOrientation;
				m_updateFBO = true;

				if (m_interactionMode == TRANSFORM_ENTITY)
				{
					rotMat = m_params.viewMat.transposed() * rotMat * m_params.viewMat;

					//feedback for 'interactive transformation' mode
					/*emit*/ if (rotation) rotation(rotMat);
				}
				else
				{
					rotateBaseViewMat(rotMat);

					showPivotSymbol(true);

					//feedback for 'echo' mode
					/*emit*/ if (viewMatRotated) viewMatRotated(rotMat);
				}
			}
		}
	}

	m_cursorMoved = true;
	m_lastMousePos.x = event->x;
	m_lastMousePos.y = event->y;

	//event->accept();

	if (m_interactionMode != TRANSFORM_ENTITY)
		updateGL();
}

void ccGLWindow::mouseReleaseEvent(SDL_MouseButtonEvent *event)
{
	if (m_interactionMode == SEGMENT_ENTITY || m_interactionMode == DRAW_CUBE)
	{
		/*emit*/ if (buttonReleased) buttonReleased();
		return;
	}

	bool cursorHasMoved = m_cursorMoved;
	bool acceptEvent = false;

	//reset to default state
	m_cursorMoved = false;
	m_lodActivated = false;

	if (m_pivotSymbolShown)
	{
		if (m_pivotVisibility == PIVOT_SHOW_ON_MOVE)
			toBeRefreshed();
		showPivotSymbol(m_pivotVisibility == PIVOT_ALWAYS_SHOW);
	}

	if (event->button == SDL_BUTTON_RIGHT)
	{
		if (!cursorHasMoved)
		{
			//specific case: interaction with item(s)
			updateActiveItemsList(event->x,event->y,false);
			if (!m_activeItems.empty())
			{
				ccInteractor* item = m_activeItems.front();
				m_activeItems.clear();
				if (item->acceptClick(event->x,height()-1-event->y,SDL_BUTTON_RIGHT))
				{
					acceptEvent = true;
					toBeRefreshed();
				}
			}
		}
		else
		{
			acceptEvent = true;
			toBeRefreshed();
		}
	}
	else if (event->button == SDL_BUTTON_LEFT)
	{
		if (cursorHasMoved)
		{
			//if a rectangular picking area has been defined
			if (m_rectPickingPoly)
			{
				CCLib::GenericIndexedCloudPersist* vertices = m_rectPickingPoly->getAssociatedCloud();
				assert(vertices);
				const CCVector3* A = vertices->getPointPersistentPtr(0);
				const CCVector3* C = vertices->getPointPersistentPtr(2);

				int pickX = (int)(A->x+C->x)/2;
				int pickY = (int)(A->y+C->y)/2;
				int pickW = (int)fabs(C->x-A->x);
				int pickH = (int)fabs(C->y-A->y);

				removeFromOwnDB(m_rectPickingPoly);
				m_rectPickingPoly=0;
				delete vertices;
				vertices=0;

				startPicking(ENTITY_RECT_PICKING, pickX+width()/2, height()/2-pickY, pickW, pickH);
			}

			toBeRefreshed();
			acceptEvent = true;
		}
		else
		{
			//picking?
			if (ccTimer::Msec()-m_lastClickTime_ticks < CC_MAX_PICKING_CLICK_DURATION_MS)
			{
				int x = event->x;
				int y = event->y;

				//specific case: interaction with item(s)
				//DGM TODO: to activate if some items take left clicks into account!
				/*if (!m_activeItems.empty())
				{
					for (std::list<ccInteractor*>::iterator it=m_activeItems.begin(); it!=m_activeItems.end(); ++it)
					if ((*it)->acceptClick(x,y,Qt::LeftButton))
					{
						event->accept();
						redraw();
						return;
					}
				}
				//*/

				bool hotZoneClick = false;
				if (m_hotZoneActivated)
				{
					if (y >= m_hotZoneMinusIconROI[1] && y <= m_hotZoneMinusIconROI[3])
					{
						if (x >= m_hotZoneMinusIconROI[0] && x <= m_hotZoneMinusIconROI[2])
						{
							if (m_params.defaultPointSize>1)
							{
								setPointSize(m_params.defaultPointSize-1);
								updateGL();
							}
							hotZoneClick=true;
						}
						else if (x >= m_hotZonePlusIconROI[0] && x <= m_hotZonePlusIconROI[2])
						{
							if (m_params.defaultPointSize<10)
							{
								setPointSize(m_params.defaultPointSize+1);
								updateGL();
							}
							hotZoneClick=true;
						}
					}

					acceptEvent = true;
				}

				if (!hotZoneClick && m_pickingMode != NO_PICKING && m_interactionMode != TRANSFORM_ENTITY)
				{
					PICKING_MODE pickingMode = m_pickingMode;
					const Uint8 *state = SDL_GetKeyboardState(NULL);

					if (pickingMode == ENTITY_PICKING && (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT]))
						pickingMode = AUTO_POINT_PICKING; //shift+click = point/triangle picking

					startPicking(pickingMode,event->x,event->y);

					/*emit*/ if (leftButtonClicked) leftButtonClicked(event->x, event->y);

					acceptEvent = true;
				}
			}
		}

		m_activeItems.clear();
	}

	/*	if (acceptEvent)
		event->accept();
	else
	event->ignore();*/

	refresh();
}

void ccGLWindow::wheelEvent(SDL_MouseWheelEvent* event)
{
	if (m_interactionMode == SEGMENT_ENTITY)
	{
		return;
	}
#ifdef __EMSCRIPTEN__
	float wheelDelta_deg = (float)event->y / 8;
#else
	float wheelDelta_deg = (float)event->y;
#endif
	onWheelEvent(wheelDelta_deg); 

	/*emit*/ if (mouseWheelRotated) mouseWheelRotated(wheelDelta_deg);
}

void ccGLWindow::onWheelEvent(float wheelDelta_deg)
{
	//in perspective mode, wheel event corresponds to 'walking'
	if (m_params.perspectiveView)
	{
		//convert degrees in 'constant' walking speed in ... pixels ;)
		static const float c_deg2PixConversion = 1.0f;
		moveCamera(0.0f,0.0f,-(c_deg2PixConversion * wheelDelta_deg) * m_params.pixelSize);
	}
	else //ortho. mode
	{
		//convert degrees in zoom 'power'
		static const float c_defaultDeg2Zoom = 20.0f;
		float zoomFactor = pow(1.1f,wheelDelta_deg / c_defaultDeg2Zoom);
		updateZoom(zoomFactor);
	}

	redraw();
}

int ccGLWindow::startCPUBasedPointPicking(int centerX, int centerY)
{
	ccHObject* nearestEntity = 0;
	int nearestElementIndex = -1;
	double nearestElementSquareDist = -1.0;

	bool autoComputeOctree = true;
	bool firstCloudWithoutOctree = false;

    int VP[4];
    getViewportArray(VP);
    const double* MM = getModelViewMatd(); //viewMat
    const double* MP = getProjectionMatd(); //projMat

	try
	{
		ccHObject::Container toProcess;
		if (m_globalDBRoot)
			toProcess.push_back(m_globalDBRoot);
		if (m_winDBRoot)
			toProcess.push_back(m_winDBRoot);

		while (!toProcess.empty())
		{
			//get next item
			ccHObject* ent = toProcess.back();
			toProcess.pop_back();

			if (!ent->isEnabled())
				continue;

			bool ignoreSubmeshes = false;

			//we look for point cloud displayed in this window
			if (ent->isVisible() && ent->getDisplay() == this)
			{
				if (ent->isKindOf(CC_MESH))
				{
					ignoreSubmeshes = true;

					ccMesh* mesh = static_cast<ccMesh*>(ent);

					int nearestTriIndex = -1;
					double nearestSquareDist = 0;

					if (mesh->trianglePicking(	centerX, height() - 1 - centerY,
												MM, MP, VP,
												nearestTriIndex,
												nearestSquareDist) )
					{
						if (nearestElementIndex < 0 || (nearestTriIndex >= 0 && nearestSquareDist < nearestElementSquareDist))
						{
							nearestElementSquareDist = nearestSquareDist;
							nearestElementIndex = nearestTriIndex;
							nearestEntity = mesh;
						}
					}
				}
			}

			if (!ignoreSubmeshes)
			{
				//add children
				for (unsigned i=0; i<ent->getChildrenNumber(); ++i)
				{
					toProcess.push_back(ent->getChild(i));
				}
			}
		}
	}
	catch (const std::bad_alloc&)
	{
		//not enough memory
		ccLog::Warning("[Picking][CPU] Not enough memory!");
	}

	//qint64 dt = m_timer.elapsed() - t0;
	//ccLog::Print(QString("[Picking][CPU] Time: %1 ms").arg(dt));

	//we must always emit a signal!
	if (entitySelectionChanged) entitySelectionChanged(nearestEntity == NULL ? -1 : nearestEntity->getUniqueID());
	return nearestEntity == NULL ? -1 : nearestEntity->getUniqueID();
}

int ccGLWindow::startPicking(PICKING_MODE pickingMode, int centerX, int centerY, int pickWidth, int pickHeight, int* subID/*=0*/)
{
	if (subID)
		*subID = -1;
	if (!m_globalDBRoot && !m_winDBRoot)
		return -1;

	assert(m_interactionMode != TRANSFORM_ENTITY);

#ifdef __EMSCRIPTEN__
	if (pickingMode == ENTITY_PICKING) {
		// opengl based picking does not work correctly with gl4es
		return startCPUBasedPointPicking(centerX, centerY);
	}
#endif

	//setup rendering context
	CC_DRAW_CONTEXT context;
	getContext(context);
	unsigned short pickingFlags = CC_DRAW_FOREGROUND;

	switch(pickingMode)
	{
	case ENTITY_PICKING:
	case ENTITY_RECT_PICKING:
		pickingFlags |= CC_DRAW_ENTITY_NAMES;
		break;
	case FAST_PICKING:
		pickingFlags |= CC_DRAW_ENTITY_NAMES;
		pickingFlags |= CC_DRAW_FAST_NAMES_ONLY;
		break;
	case POINT_PICKING:
		pickingFlags |= CC_DRAW_POINT_NAMES;	//automatically push entity names as well!
		break;
	case TRIANGLE_PICKING:
		pickingFlags |= CC_DRAW_TRI_NAMES;		//automatically push entity names as well!
		break;
	case AUTO_POINT_PICKING:
		pickingFlags |= CC_DRAW_POINT_NAMES;	//automatically push entity names as well!
		pickingFlags |= CC_DRAW_TRI_NAMES;
		break;
	default:
		return -1;
	}
	makeCurrent();

	//no need to clear display, we don't draw anything new!
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//setup selection buffers
	memset(m_pickingBuffer,0,sizeof(GLuint)*CC_PICKING_BUFFER_SIZE);
	glSelectBuffer(CC_PICKING_BUFFER_SIZE,m_pickingBuffer);
	glRenderMode(GL_SELECT);
	glInitNames();

	//get viewport
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);

	//3D objects picking
	{
		context.flags = CC_DRAW_3D | pickingFlags;

		glEnable(GL_DEPTH_TEST);

		//projection matrix
		glMatrixMode(GL_PROJECTION);
		//restrict drawing to the picking area
		glLoadIdentity();
		gluPickMatrix((GLdouble)centerX,(GLdouble)(viewport[3]-centerY),(GLdouble)pickWidth,(GLdouble)pickHeight,viewport);
		glMultMatrixd(getProjectionMatd());

		//model view matrix
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixd(getModelViewMatd());

		//display 3D objects
		if (m_globalDBRoot)
			m_globalDBRoot->draw(context);
		if (m_winDBRoot)
			m_winDBRoot->draw(context);

		ccGLUtils::CatchGLError("ccGLWindow::startPicking.draw(3D)");
	}

	//2D objects picking
	if (pickingMode == ENTITY_PICKING || pickingMode == ENTITY_RECT_PICKING || pickingMode == FAST_PICKING)
	{
		context.flags = CC_DRAW_2D | pickingFlags;

		glDisable(GL_DEPTH_TEST);

		//we must first grab the 2D ortho view projection matrix
		setStandardOrthoCenter();
		glMatrixMode(GL_PROJECTION);
		double orthoProjMatd[OPENGL_MATRIX_SIZE];
		glGetDoublev(GL_PROJECTION_MATRIX, orthoProjMatd);
		//restrict drawing to the picking area
		glLoadIdentity();
		gluPickMatrix((GLdouble)centerX,(GLdouble)(viewport[3]-centerY),(GLdouble)pickWidth,(GLdouble)pickHeight,viewport);
		glMultMatrixd(orthoProjMatd);
		glMatrixMode(GL_MODELVIEW);

		//we display 2D objects
		if (m_globalDBRoot)
			m_globalDBRoot->draw(context);
		if (m_winDBRoot)
			m_winDBRoot->draw(context);

		ccGLUtils::CatchGLError("ccGLWindow::startPicking.draw(2D)");
	}

	glFlush();

	// returning to normal rendering mode
	int hits = glRenderMode(GL_RENDER);

	ccGLUtils::CatchGLError("ccGLWindow::startPicking.render");

	ccLog::PrintDebug("Picking hits: %i",hits);
	if (hits<0)
	{
		ccLog::Warning("Too many items inside picking zone! Try to zoom in...");
		return -1;
	}

	//process hits
	int selectedID=-1,subSelectedID=-1;
	std::set<int> selectedIDs; //for ENTITY_RECT_PICKING mode only
	{
		GLuint minMinDepth = (~0);
		const GLuint* _selectBuf = m_pickingBuffer;
		for (int i=0;i<hits;++i)
		{
			const GLuint& n = _selectBuf[0]; //number of names on stack
			if (n) //if we draw anything outside of 'glPushName()... glPopName()' then it will appear here with as an empty set!
			{
				//n should be esqual to 1 (CC_DRAW_ENTITY_NAMES mode) or 2 (CC_DRAW_POINT_NAMES/CC_DRAW_TRIANGLES_NAMES modes)!
				assert(n==1 || n==2);
				const GLuint& minDepth = _selectBuf[1];
				//const GLuint& maxDepth = _selectBuf[2];
				const GLuint& currentID = _selectBuf[3];

				if (pickingMode == ENTITY_RECT_PICKING)
				{
					//pick them all!
					selectedIDs.insert(currentID);
				}
				else
				{
					//if there are multiple hits, we keep only the nearest
					if (selectedID < 0 || minDepth < minMinDepth)
					{
						selectedID = currentID;
						subSelectedID = (n>1 ? _selectBuf[4] : -1);
						minMinDepth = minDepth;
					}
				}
			}

			_selectBuf += (3+n);
		}
	}

	if (subID)
		*subID = subSelectedID;

	//standard "entity" picking
	if (pickingMode == ENTITY_PICKING)
	{
		/*emit*/ if (entitySelectionChanged) entitySelectionChanged(selectedID);
	}
	//rectangular "entity" picking
	else if (pickingMode == ENTITY_RECT_PICKING)
	{
		/*emit*/ if (entitiesSelectionChanged) entitiesSelectionChanged(selectedIDs);
	}
	//"3D point" picking
	else if (pickingMode == POINT_PICKING)
	{
		if (selectedID>=0 && subSelectedID>=0)
		{
			/*emit*/ if (pointPicked) pointPicked(selectedID,(unsigned)subSelectedID,centerX,centerY);
		}
	}
	else if (pickingMode == AUTO_POINT_PICKING)
	{
		if (m_globalDBRoot && selectedID>=0 && subSelectedID>=0)
		{
			ccHObject* obj = m_globalDBRoot->find(selectedID);
			if (obj)
			{
				//auto spawn the right label
				cc2DLabel* label = 0;
				if (obj->isKindOf(CC_POINT_CLOUD))
				{
					label = new cc2DLabel();
					label->addPoint(static_cast<ccGenericPointCloud*>(obj),subSelectedID);
					obj->addChild(label,true);
				}
				else if (obj->isKindOf(CC_MESH))
				{
					label = new cc2DLabel();
					ccGenericMesh *mesh = static_cast<ccGenericMesh*>(obj);
					ccGenericPointCloud *cloud = mesh->getAssociatedCloud();
					assert(cloud);
					CCLib::TriangleSummitsIndexes *summitsIndexes = mesh->getTriangleIndexes(subSelectedID);
					label->addPoint(cloud,summitsIndexes->i1);
					label->addPoint(cloud,summitsIndexes->i2);
					label->addPoint(cloud,summitsIndexes->i3);
					cloud->addChild(label,true);
					if (!cloud->isEnabled())
					{
						cloud->setVisible(false);
						cloud->setEnabled(true);
					}
				}

				if (label)
				{
					label->setVisible(true);
					label->setDisplay(obj->getDisplay());
					label->setPosition((float)(centerX+20)/(float)width(),(float)(centerY+20)/(float)height());
					/*emit*/ if (newLabel) newLabel(static_cast<ccHObject*>(label));
					processEvents();

					toBeRefreshed();
				}
			}
		}
	}

	return selectedID;
}

void ccGLWindow::displayNewMessage(const std::string& message,
								   MessagePosition pos,
								   bool append/*=false*/,
								   int displayMaxDelay_sec/*=2*/,
								   MessageType type/*=CUSTOM_MESSAGE*/)
{
	if (message.length() == 0)
	{
		if (!append)
		{
			std::list<MessageToDisplay>::iterator it = m_messagesToDisplay.begin();
			while (it != m_messagesToDisplay.end())
			{
				//same position? we remove the message
				if (it->position == pos)
					it = m_messagesToDisplay.erase(it);
				else
					++it;
			}
		}
		else
		{
			ccLog::Warning("[ccGLWindow::displayNewMessage] Appending an empty message has no effect!"); 
		}
		return;
	}

	//shall we replace the equivalent message(if any)?
	if (!append)
	{
		//only if type is not 'custom'
		if (type != CUSTOM_MESSAGE)
		{
			for (std::list<MessageToDisplay>::iterator it = m_messagesToDisplay.begin(); it != m_messagesToDisplay.end();)
			{
				//same type? we remove it
				if (it->type == type)
					it = m_messagesToDisplay.erase(it);
				else
					++it;
			}
		}
		else
		{
			ccLog::WarningDebug("[ccGLWindow::displayNewMessage] Append is forced for custom messages!");
		}
	}
	else
	{
		if (pos == SCREEN_CENTER_MESSAGE)
		{
			ccLog::Warning("[ccGLWindow::displayNewMessage] Append is not supported for center screen messages!"); 
			append = false;
		}
	}

	MessageToDisplay mess;
	mess.message = message;
	mess.messageValidity_sec = ccTimer::Sec()+displayMaxDelay_sec;
	mess.position = pos;
	mess.type = type;
	m_messagesToDisplay.push_back(mess);

	//ccLog::Print(std::string("[displayNewMessage] New message valid until %1 s.").arg(mess.messageValidity_sec));
}

void ccGLWindow::setPointSize(float size)
{
	m_params.defaultPointSize = size;
	m_updateFBO = true;
}

void ccGLWindow::setLineWidth(float width)
{
	m_params.defaultLineWidth = width;
	m_updateFBO = true;
}

void ccGLWindow::glEnableSunLight()
{
	glLightfv(GL_LIGHT0,GL_DIFFUSE,ccGui::Parameters().lightDiffuseColor);
	glLightfv(GL_LIGHT0,GL_AMBIENT,ccGui::Parameters().lightAmbientColor);
	glLightfv(GL_LIGHT0,GL_SPECULAR,ccGui::Parameters().lightSpecularColor);
	glLightfv(GL_LIGHT0, GL_POSITION, m_sunLightPos);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
	glEnable(GL_LIGHT0);
}

void ccGLWindow::glDisableSunLight()
{
	glDisable(GL_LIGHT0);
}

void ccGLWindow::setSunLight(bool state)
{
	m_sunLightEnabled=state;
	displayNewMessage(state ? "Sun light ON" : "Sun light OFF",
						ccGLWindow::LOWER_LEFT_MESSAGE,
						false,
						2,
						SUN_LIGHT_STATE_MESSAGE);
	redraw();
#if 0
	//save parameter
	{
		QSettings settings;
		settings.beginGroup(c_ps_groupName);
		settings.setValue(c_ps_sunLight,	m_sunLightEnabled);
	}
#endif
}

void ccGLWindow::toggleSunLight()
{
	setSunLight(!m_sunLightEnabled);
}

void ccGLWindow::glEnableCustomLight()
{
	glLightfv(GL_LIGHT1,GL_DIFFUSE,ccGui::Parameters().lightDiffuseColor);
	glLightfv(GL_LIGHT1,GL_AMBIENT,ccGui::Parameters().lightAmbientColor);
	glLightfv(GL_LIGHT1,GL_SPECULAR,ccGui::Parameters().lightSpecularColor);
	glLightfv(GL_LIGHT1,GL_POSITION,m_customLightPos);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
	glEnable(GL_LIGHT1);
}

void ccGLWindow::glDisableCustomLight()
{
	glDisable(GL_LIGHT1);
}

void ccGLWindow::setCustomLight(bool state)
{
	m_customLightEnabled=state;
	displayNewMessage(state ? "Custom light ON" : "Custom light OFF",
						ccGLWindow::LOWER_LEFT_MESSAGE,
						false,
						2,
						CUSTOM_LIGHT_STATE_MESSAGE);

	invalidateViewport();
	redraw();
#if 0
	//save parameter
	{
		QSettings settings;
		settings.beginGroup(c_ps_groupName);
		settings.setValue(c_ps_customLight,	m_customLightEnabled);
	}
#endif
}

void ccGLWindow::toggleCustomLight()
{
	setCustomLight(!m_customLightEnabled);
}

void ccGLWindow::drawCustomLight()
{
	glColor3ubv(ccColor::yellow);
	//ensure that the star size is constant (in pixels)
	GLfloat d = CC_DISPLAYED_CUSTOM_LIGHT_LENGTH * computeActualPixelSize();

	glBegin(GL_LINES);
	glVertex3f(m_customLightPos[0]-d, m_customLightPos[1],   m_customLightPos[2]);
	glVertex3f(m_customLightPos[0]+d, m_customLightPos[1],   m_customLightPos[2]);
	glVertex3f(m_customLightPos[0],   m_customLightPos[1]-d, m_customLightPos[2]);
	glVertex3f(m_customLightPos[0],   m_customLightPos[1]+d, m_customLightPos[2]);
	glVertex3f(m_customLightPos[0],   m_customLightPos[1],   m_customLightPos[2]-d);
	glVertex3f(m_customLightPos[0],   m_customLightPos[1],   m_customLightPos[2]+d);
	glEnd();
}

//draw a unit circle in a given plane (0=YZ, 1 = XZ, 2=XY) 
void glDrawUnitCircle(unsigned char dim, int steps = 64)
{
	float thetaStep = static_cast<float>(2.0*M_PI/(double)steps);
	float theta = 0.0f;
	unsigned char dimX = (dim<2 ? dim+1 : 0);
	unsigned char dimY = (dimX<2 ? dimX+1 : 0);

	CCVector3 P(0.0f);

	glBegin(GL_LINE_LOOP);
	for (int i=0;i<steps;++i)
	{
		P.u[dimX] = cos(theta);
		P.u[dimY] = sin(theta);
		glVertex3fv(P.u);
		theta += thetaStep;
	}
	glEnd();
}

void ccGLWindow::setPivotVisibility(PivotVisibility vis)
{
	m_pivotVisibility = vis;
#if 0 // TODO
	//auto-save last pivot visibility settings
	{
		QSettings settings;
		settings.beginGroup(c_ps_groupName);
		settings.setValue(c_ps_pivotVisibility,	vis);
		settings.endGroup();
	}
#endif
}

ccGLWindow::PivotVisibility ccGLWindow::getPivotVisibility() const
{
	return m_pivotVisibility;
}

void ccGLWindow::showPivotSymbol(bool state)
{
	//is the pivot really going to be drawn?
	if (state && !m_pivotSymbolShown && m_params.objectCenteredView && m_pivotVisibility != PIVOT_HIDE)
	{
		invalidateViewport();
	}

	m_pivotSymbolShown = state;
}

void ccGLWindow::drawPivot()
{
	if (!m_params.objectCenteredView)
		return;

	if (m_pivotVisibility == PIVOT_HIDE ||
		m_pivotVisibility == PIVOT_SHOW_ON_MOVE && !m_pivotSymbolShown)
		return;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	//place origin on pivot point
	glTranslatef(m_params.pivotPoint.x, m_params.pivotPoint.y, m_params.pivotPoint.z);

	//compute actual symbol radius
	float symbolRadius = CC_DISPLAYED_PIVOT_RADIUS_PERCENT * (float)std::min(m_glWidth,m_glHeight) * 0.5f;

	if (m_pivotGLList == GL_INVALID_LIST_ID)
	{
		m_pivotGLList = glGenLists(1);
		glNewList(m_pivotGLList, GL_COMPILE);

		//draw a small sphere
		{
			ccSphere sphere(10.0f/symbolRadius);
			sphere.setColor(ccColor::yellow);
			sphere.showColors(true);
			sphere.setVisible(true);
			sphere.setEnabled(true);
			//force lighting for proper sphere display
			glPushAttrib(GL_LIGHTING_BIT);
			glEnableSunLight();
			CC_DRAW_CONTEXT context;
			getContext(context);
			context.flags = CC_DRAW_3D | CC_DRAW_FOREGROUND | CC_LIGHT_ENABLED;
			context._win = 0;
			sphere.draw(context);
			glPopAttrib();
		}

		//draw 3 circles
		glPushAttrib(GL_LINE_BIT);
		glEnable(GL_LINE_SMOOTH);
		glPushAttrib(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);
		const float c_alpha = 0.6f;
		glLineWidth(2.0f);

		//pivot symbol: 3 circles
		glColor4f(1.0f,0.0f,0.0f,c_alpha);
		glDrawUnitCircle(0);
		glBegin(GL_LINES);
		glVertex3f(-1.0f,0.0f,0.0f);
		glVertex3f( 1.0f,0.0f,0.0f);
		glEnd();

		glColor4f(0.0f,1.0f,0.0f,c_alpha);
		glDrawUnitCircle(1);
		glBegin(GL_LINES);
		glVertex3f(0.0f,-1.0f,0.0f);
		glVertex3f(0.0f, 1.0f,0.0f);
		glEnd();

		glColor4f(0.0f,0.7f,1.0f,c_alpha);
		glDrawUnitCircle(2);
		glBegin(GL_LINES);
		glVertex3f(0.0f,0.0f,-1.0f);
		glVertex3f(0.0f,0.0f, 1.0f);
		glEnd();

		glPopAttrib();
		glPopAttrib();

		glEndList();
	}

	//constant scale
	float scale = symbolRadius * computeActualPixelSize();
	glScalef(scale,scale,scale);
	
	glCallList(m_pivotGLList);

	glPopMatrix();
}

void ccGLWindow::togglePerspective(bool objectCentered)
{
	if (m_params.objectCenteredView != objectCentered)
		setPerspectiveState(true,objectCentered);
	else
		setPerspectiveState(!m_params.perspectiveView,objectCentered);
}

float ccGLWindow::computeActualPixelSize() const
{
	if (!m_params.perspectiveView)
	{
		return m_params.pixelSize / m_params.zoom;
	}

	int minScreenDim = std::min(m_glWidth,m_glHeight);
	if (minScreenDim <= 0)
		return 1.0f;

	//Camera center to pivot vector
	float zoomEquivalentDist = (m_params.cameraCenter - m_params.pivotPoint).norm();

	return (zoomEquivalentDist * tan(m_params.fov*CC_DEG_TO_RAD) / minScreenDim);
}

float ccGLWindow::computePerspectiveZoom() const
{
	if (!m_params.perspectiveView)
		return m_params.zoom;
	//we compute the zoom equivalent to the corresponding camera position (inverse of above calculus)
	if (m_params.fov < ZERO_TOLERANCE)
		return 1.0;

	//Camera center to pivot vector
	float zoomEquivalentDist = (m_params.cameraCenter - m_params.pivotPoint).norm();
	if (zoomEquivalentDist < ZERO_TOLERANCE)
		return 1.0f;
	
	float screenSize = static_cast<float>(std::min(m_glWidth,m_glHeight))*m_params.pixelSize; //see how pixelSize is computed!
	return screenSize / (zoomEquivalentDist * tan(m_params.fov*CC_DEG_TO_RAD));
}

void ccGLWindow::setPerspectiveState(bool state, bool objectCenteredView)
{
	//precedent state
	bool perspectiveWasEnabled = m_params.perspectiveView;
	bool viewWasObjectCentered = m_params.objectCenteredView;

	//new state
	m_params.perspectiveView = state;
	m_params.objectCenteredView = objectCenteredView;

	//Camera center to pivot vector
	CCVector3 PC = m_params.cameraCenter - m_params.pivotPoint;

	if (m_params.perspectiveView)
	{
		if (!perspectiveWasEnabled) //from ortho. mode to perspective view
		{
			//we compute the camera position that gives 'quite' the same view as the ortho one
			//(i.e. we replace the zoom by setting the camera at the right distance from
			//the pivot point)
			assert(m_params.fov > ZERO_TOLERANCE);
			float screenSize = static_cast<float>(std::min(m_glWidth,m_glHeight))*m_params.pixelSize; //see how pixelSize is computed!
			PC.z = screenSize/(m_params.zoom*tan(m_params.fov*CC_DEG_TO_RAD));
		}

		//display message
		displayNewMessage(objectCenteredView ? "Centered perspective ON" : "Viewer-based perspective ON",
														ccGLWindow::LOWER_LEFT_MESSAGE,
														false,
														2,
														PERSPECTIVE_STATE_MESSAGE);
	}
	else
	{
		m_params.objectCenteredView = true; //object-centered mode is forced for otho. view
		m_params.aspectRatio = 1.0; //TODO: is it really necessary?

		if (perspectiveWasEnabled) //from perspective view to ortho. view
		{
			//we compute the zoom equivalent to the corresponding camera position (inverse of above calculus)
			float newZoom = computePerspectiveZoom();
			setZoom(newZoom);
		}

		displayNewMessage("Perspective OFF",
							ccGLWindow::LOWER_LEFT_MESSAGE,
							false,
							2,
							PERSPECTIVE_STATE_MESSAGE);
	}

	//if we change form object-based to viewer-based visualization, we must
	//'rotate' around the object (or the opposite ;)
	if (viewWasObjectCentered && !m_params.objectCenteredView)
		m_params.viewMat.transposed().apply(PC); //inverse rotation
	else if (!viewWasObjectCentered && m_params.objectCenteredView)
		m_params.viewMat.apply(PC);

	setCameraPos(m_params.pivotPoint + PC);

	/*emit*/ if (perspectiveStateChanged) perspectiveStateChanged();
#if 0
	//auto-save last perspective settings
	{
		QSettings settings;
		settings.beginGroup(c_ps_groupName);
		//write parameters
		settings.setValue(c_ps_perspectiveView, m_params.perspectiveView);
		settings.setValue(c_ps_objectMode, m_params.objectCenteredView);
		settings.endGroup();
	}
#endif
	invalidateViewport();
	invalidateVisualization();
}

void ccGLWindow::setFov(float fov)
{
	if (fov < ZERO_TOLERANCE)
	{
		ccLog::Warning("[ccGLWindow::setFov] Invalid FOV value!");
		return;
	}

	if (m_params.fov != fov)
	{
		//update param
		m_params.fov = fov;
		//and camera state (if perspective view is 'on')
		if (m_params.perspectiveView)
		{
			invalidateViewport();
			invalidateVisualization();
		}
	}
}

void ccGLWindow::setViewportParameters(const ccViewportParameters& params)
{
	ccViewportParameters oldParams = m_params;
	m_params = params;

	invalidateViewport();
	invalidateVisualization();

	/*emit*/ if (baseViewMatChanged) baseViewMatChanged(m_params.viewMat);
	/*emit*/ if (pivotPointChanged) pivotPointChanged(m_params.pivotPoint);
	/*emit*/ if (cameraPosChanged) cameraPosChanged(m_params.cameraCenter);
}

void ccGLWindow::getViewportArray(int vpArray[])
{
	makeCurrent();
	glGetIntegerv(GL_VIEWPORT, vpArray);
}

void ccGLWindow::rotateBaseViewMat(const ccGLMatrix& rotMat)
{
	m_params.viewMat = rotMat * m_params.viewMat;

	//we emit the 'baseViewMatChanged' signal
	/*emit*/ if (baseViewMatChanged) baseViewMatChanged(m_params.viewMat);

	invalidateVisualization();
}

void ccGLWindow::updateZoom(float zoomFactor)
{
	//no 'zoom' in viewer based perspective
	assert(!m_params.perspectiveView);

	if (zoomFactor>0.0 && zoomFactor!=1.0)
		setZoom(m_params.zoom*zoomFactor);
}

void ccGLWindow::setCustomView(const CCVector3& forward, const CCVector3& up, bool forceRedraw/*=true*/)
{
	makeCurrent();

	bool wasViewerBased = !m_params.objectCenteredView;
	if (wasViewerBased)
		setPerspectiveState(m_params.perspectiveView,true);

	CCVector3 uForward = forward; uForward.normalize();

	CCVector3 uSide = uForward.cross(up);
	uSide.normalize();
	CCVector3 uUp = uSide.cross(uForward);
	uUp.normalize();

	float* mat = m_params.viewMat.data();
	mat[0] = uSide.x; mat[4] = uSide.y; mat[8] = uSide.z;
	mat[1] = uUp.x; mat[5] = uUp.y; mat[9] = uUp.z;
	mat[2] = -uForward.x; mat[6] = -uForward.y; mat[10] = -uForward.z;

	if (wasViewerBased)
		setPerspectiveState(m_params.perspectiveView,false);

	invalidateVisualization();

	//we emit the 'baseViewMatChanged' signal
	/*emit*/ if (baseViewMatChanged) baseViewMatChanged(m_params.viewMat);

	if (forceRedraw)
		redraw();
}

void ccGLWindow::setView(CC_VIEW_ORIENTATION orientation, bool forceRedraw/*=true*/)
{
	makeCurrent();

	bool wasViewerBased = !m_params.objectCenteredView;
	if (wasViewerBased)
		setPerspectiveState(m_params.perspectiveView,true);

	m_params.viewMat = ccGLUtils::GenerateViewMat(orientation);

	if (wasViewerBased)
		setPerspectiveState(m_params.perspectiveView,false);

	invalidateVisualization();

	//we emit the 'baseViewMatChanged' signal
	/*emit*/ if (baseViewMatChanged) baseViewMatChanged(m_params.viewMat);

	if (forceRedraw)
		redraw();
}

void ccGLWindow::removeFBO()
{
	//we "disconnect" current FBO, to avoid wrong display/errors
	//if QT tries to redraw window during object destruction
	ccFrameBufferObject* _fbo = m_fbo;
	m_fbo=0;

	if (_fbo)
		delete _fbo;
}

bool ccGLWindow::initFBO(int w, int h)
{
	//we "disconnect" current FBO, to avoid wrong display/errors
	//if QT tries to redraw window during initialization
	ccFrameBufferObject* _fbo = m_fbo;
	m_fbo=0;

	if (!_fbo)
		_fbo = new ccFrameBufferObject();

	bool success=false;
	if (_fbo->init(w,h))
	{
		if (_fbo->initTexture(0,GL_RGBA,GL_RGBA,GL_FLOAT))
			success = _fbo->initDepth(GL_CLAMP_TO_BORDER,GL_DEPTH_COMPONENT32,GL_NEAREST,GL_TEXTURE_2D);
	}

	if (!success)
	{
		ccLog::Warning("[FBO] Initialization failed!");
		delete _fbo;
		_fbo=0;
		m_alwaysUseFBO=false;
		return false;
	}

	//ccLog::Print("[FBO] Initialized");

	m_fbo = _fbo;
	m_updateFBO = true;

	return true;
}

void ccGLWindow::removeGLFilter()
{
	//we "disconnect" current glFilter, to avoid wrong display/errors
	//if QT tries to redraw window during object destruction
	ccGlFilter* _filter = m_activeGLFilter;
	m_activeGLFilter=0;

	if (_filter)
		delete _filter;
}

bool ccGLWindow::initGLFilter(int w, int h)
{
	if (!m_activeGLFilter)
		return false;

	//we "disconnect" current glFilter, to avoid wrong display/errors
	//if QT tries to redraw window during initialization
	ccGlFilter* _filter = m_activeGLFilter;
	m_activeGLFilter=0;

	std::string shadersPath = ccGLWindow::getShadersPath();

	//ccLog::Print(QString("Shaders path: %1").arg(shadersPath));

	if (!_filter->init(w,h,shadersPath.c_str()))
	{
		ccLog::Warning("[GL Filter] Initialization failed!");
		return false;
	}

	ccLog::Print("[GL Filter] Filter initialized");

	m_activeGLFilter = _filter;

	return true;
}

void ccGLWindow::display3DLabel(const std::string& str, const CCVector3& pos3D, const unsigned char* rgb/*=0*/)
{
	// TODO
}

void ccGLWindow::displayText(std::string text, int x, int y, unsigned char align/*=ALIGN_HLEFT | ALIGN_VTOP*/, unsigned char bkgAlpha/*=0*/, const unsigned char* rgbColor/*=0*/)
{
	makeCurrent();

	int x2 = x;
	int y2 = m_glHeight-1-y;

	//FIXME: doesn't work!
	//if (m_captureModeZoomFactor != 1.0f)
	//{
	//	x2 = (int)(m_captureModeZoomFactor * (float)x2);
	//	y2 = (int)(m_captureModeZoomFactor * (float)y2);
	//}

	//Some versions of Qt seem to need glColorf instead of glColorub! (see https://bugreports.qt-project.org/browse/QTBUG-6217)
	const unsigned char* col = (rgbColor ? rgbColor : ccGui::Parameters().textDefaultCol);

	int margin = 15/4;

	if (align != (ALIGN_HLEFT | ALIGN_VTOP) || bkgAlpha != 0)
	{
//		QRect rect = fm.boundingRect(text);
		int width = toGB2312(text).length() * 8;
		int height = 16;

		//text alignment
		if (align & ALIGN_HMIDDLE)
			x2 -= width/2;
		else if (align & ALIGN_HRIGHT)
			x2 -= width;
		if (align & ALIGN_VMIDDLE)
			y2 += height/2;
		else if (align & ALIGN_VBOTTOM)
			y2 += height;

		//background is not totally transparent
		if (bkgAlpha != 0)
		{
			//inverted color with a bit of transparency
			glPushAttrib(GL_COLOR_BUFFER_BIT);
			glEnable(GL_BLEND);
			glColor4f(1.0f-(float)col[0]/(float)MAX_COLOR_COMP,1.0f-(float)col[1]/(float)MAX_COLOR_COMP,1.0f-(float)col[2]/(float)MAX_COLOR_COMP,(float)bkgAlpha/(float)100);
			int xB = x2 - m_glWidth/2;
			int yB = m_glHeight/2 - y2;
			yB += margin/2; //empirical compensation

			glBegin(GL_POLYGON);
			glVertex2d(xB - margin, yB - margin);
			glVertex2d(xB - margin, yB + height + margin/2);
			glVertex2d(xB + width + margin, yB + height + margin/2); 
			glVertex2d(xB + width + margin, yB - margin); 
			glEnd();
			glPopAttrib();
		}
	}

	glColor3f((float)col[0]/(float)MAX_COLOR_COMP,(float)col[1]/(float)MAX_COLOR_COMP,(float)col[2]/(float)MAX_COLOR_COMP);
	if (align & ALIGN_VBOTTOM)
		y2 -= margin; //empirical compensation
	else if (align & ALIGN_VMIDDLE)
		y2 -= margin/2; //empirical compensation
	renderText(x2, y2, text);
}

std::string ccGLWindow::getShadersPath()
{
   return "/shaders";
}

//*********** OPENGL EXTENSIONS ***********//

//! Loads all available OpenGL extensions
bool ccGLWindow::InitGLEW()
{
    #ifdef USE_GLEW
    // GLEW initialization
    GLenum code = glewInit();
    if(code != GLEW_OK)
    {
        ccLog::Error("Error while initializing OpenGL extensions! (see console)");
        ccLog::Warning("GLEW error: %s",glewGetErrorString(code));
        return false;
    }

    ccLog::Print("GLEW: initialized!");
    return true;
    #else
    return false;
    #endif
}

bool ccGLWindow::CheckExtension(const char *extName)
{
    #ifdef USE_GLEW
    return glewIsSupported(extName);
    #else
    return false;
    #endif
}

bool ccGLWindow::CheckShadersAvailability()
{
    return false;
#if 0
    bool bARBShadingLanguage       = CheckExtension("GL_ARB_shading_language_100");
    bool bARBShaderObjects         = CheckExtension("GL_ARB_shader_objects");
    bool bARBVertexShader          = CheckExtension("GL_ARB_vertex_shader");
    bool bARBFragmentShader        = CheckExtension("GL_ARB_fragment_shader");

    bool bShadersSupported = bARBShadingLanguage &&
                             bARBShaderObjects &&
                             bARBVertexShader &&
                             bARBFragmentShader;

    return bShadersSupported;
#endif
}

bool ccGLWindow::CheckFBOAvailability()
{
    return CheckExtension("GL_EXT_framebuffer_object");
}

bool ccGLWindow::CheckVBOAvailability()
{
    return CheckExtension("GL_ARB_vertex_buffer_object");
}

void ccGLWindow::makeCurrent()
{
  // TODO
}

void ccGLWindow::updateGL()
{
  // TODO
}

void ccGLWindow::processEvents()
{
}

static bool is_gb(unsigned char b1, unsigned char b2)
{
	if (b1 < 0xa1 || b1 > 0xfe)
		return false;
	if (b2 < 0xa1 || b2 > 0xfe)
		return false;
	return true;
}

void ccGLWindow::renderText(int x, int y, const std::string &str)
{
	#include "fonts/ascii.h"
	#include "fonts/gbfont.h"

	std::string gb_str = toGB2312(str);

	unsigned char *c = (unsigned char *)gb_str.c_str();

    glPushClientAttrib( GL_CLIENT_PIXEL_STORE_BIT );
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glRasterPos2i(x - m_glWidth / 2, m_glHeight / 2 - y);
	float color[4];
	glGetFloatv(GL_CURRENT_COLOR, (GLfloat *)color);
	unsigned char bitmap[16 * 16 * 4];
	while (*c) {
		// glBitmap does not work under gl4es for some reason
		if (*c & (1 << 7)) {
			unsigned char b1 = *c++;
			unsigned char b2 = *c;
			if (b2 == '\0') {
				return;
			}
			if (!is_gb(b1, b2)) {
				continue;
			}
			const unsigned char *ch = &gbfont[((b1 - 0xa1) * 94 + (b2 - 0xa1)) * 32];
			for (int i = 0; i < 16; i++) {
				for (int j = 0; j < 16; j++) {
					bool f = (ch[(15 - i) * 2 + (j >> 3)] & (1 << ((15 - j) & 7)));
					bitmap[(i * 16 + j) * 4] = (f ? (color[0] * 255) : 0);
					bitmap[(i * 16 + j) * 4 + 1] = (f ? (color[1] * 255) : 0);
					bitmap[(i * 16 + j) * 4 + 2] = (f ? (color[2] * 255) : 0);
					bitmap[(i * 16 + j) * 4 + 3] = (f ? (color[3] * 255) : 0);
				}
			}
			glRasterPos2i(x - m_glWidth / 2, m_glHeight / 2 - y);
			x += 16;
			glDrawPixels(16, 16, GL_RGBA, GL_UNSIGNED_BYTE, (const GLubyte *)bitmap);
		} else {
			for (int i = 0; i < 15; i++) {
				for (int j = 0; j < 8; j++) {
					bool f = (iso_font[(unsigned int)*c * 15 + i] & (1 << (7 - j)));
					bitmap[(i * 8 + j) * 4] = (f ? (color[0] * 255) : 0);
					bitmap[(i * 8 + j) * 4 + 1] = (f ? (color[1] * 255) : 0);
					bitmap[(i * 8 + j) * 4 + 2] = (f ? (color[2] * 255) : 0);
					bitmap[(i * 8 + j) * 4 + 3] = (f ? (color[3] * 255) : 0);
				}
			}
			glRasterPos2i(x - m_glWidth / 2, m_glHeight / 2 - y);
			x += 8;
			glDrawPixels(8, 15, GL_RGBA, GL_UNSIGNED_BYTE, (const GLubyte *)bitmap);
		}
		c++;
	}
	glPopClientAttrib();
}
