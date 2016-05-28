#define OGREOPENVR_GL
#define OGREOPENVR_DX11

#ifdef OGREOPENVR_DX11
#include "OgreD3D11RenderSystem.h"
#include "OgreD3D11Texture.h"
#include "D3D11.h"
#endif

#ifdef OGREOPENVR_GL
#undef GL_VERSION_1_1
#undef GL_VERSION_3_0
#undef GL_VERSION_3_1
#undef GL_VERSION_3_2
#undef GL_VERSION_3_3
#undef GL_VERSION_4_0
#undef GL_VERSION_4_1
#undef GL_VERSION_4_2
#undef GL_VERSION_4_3
#undef GL_VERSION_4_4
//#define GL_GLEXT_PROTOTYPES 1
#include "ogreGL3PlusRenderSystem.h"
#include "ogreGL3PlusTexture.h"
#include "OgreGL3PlusHardwarePixelBuffer.h"

#include "OgreGL3PlusPixelFormat.h"
#include "OgreGL3PlusRenderSystem.h"
#include "OgreGL3PlusHardwareBufferManager.h"
#include "OgreGL3PlusTextureBuffer.h"
#include "ogregl3plusframebufferobject.h"
#include "ogregl3plusfborendertexture.h"
#endif

// Ogre
#include "ogrecamera.h"
#include "ogrescenemanager.h"
#include "ogretexturemanager.h"
#include "ogretexture.h"
#include "OgreItem.h"
#include "ogrelogmanager.h"
#include "OgreMesh.h"
#include "OgreMeshManager.h"
#include "OgreMesh2.h"
#include "OgreMeshManager2.h"
#include "OgreMesh2Serializer.h"
#include "compositor/OgreCompositorManager2.h"
#include "compositor/OgreCompositorWorkspace.h"
#include "ogrehardwarepixelbuffer.h"
#include "ogreroot.h"
#include "ogrerendertarget.h"
#include "ogrerendertexture.h"
#include "ogrerenderwindow.h"

#include <openvr.h>
//#include "shared/Matrices.h"

#include "OgreOpenVR.h"

namespace
{
	// General
	Ogre::Vector3				g_position;
	Ogre::Quaternion			g_orientation;
	Ogre::Vector3				g_poseNeutralPosition(0.0f, 0.0f, 0.0f);
	Ogre::Quaternion			g_poseNeutralOrientation(1.0f, 0.0f, 0.0f, 0.0f);
	float						g_heightStanding = 1.7f;
	float						g_heightSitting = 1.0;
	bool						g_isStanding = true;
	float						g_IPD = 0.07f;
	float						g_fNearClip = 0.01f;
	float						g_fFarClip = 1000.0f;
	long long					g_frameIndex = 0;
	bool						g_usingGL = false;
	const unsigned int			LeftEye = 0;
	const unsigned int			RightEye = 1;
	const unsigned int			EyeCount = 2;

	// Ogre
	Ogre::Camera *				g_cameras[EyeCount] = { nullptr, nullptr };
	Ogre::SceneNode *			g_cameraNode = nullptr;
	Ogre::CompositorWorkspace *	g_workspaces[EyeCount] = { nullptr, nullptr };
	Ogre::RenderWindow *		g_window = nullptr;
	Ogre::SceneManager *		g_sceneManager = nullptr;
	Ogre::TexturePtr			g_ogreRenderTexture; // [EyeCount];
	Ogre::TexturePtr			g_ogreResolveTexture; // [EyeCount];

	// openvr
	vr::IVRSystem				*g_pHMD;
	vr::IVRRenderModels			*g_pRenderModels;
	std::string					g_strDriver;
	std::string					g_strDisplay;
	vr::TrackedDevicePose_t		g_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
	Ogre::Matrix4				g_rmat4DevicePose[vr::k_unMaxTrackedDeviceCount];
	Ogre::Matrix4				g_mat4HMDPose;
	bool						g_rbShowTrackedDevice[vr::k_unMaxTrackedDeviceCount];
	uint32_t					g_nRenderWidth;
	uint32_t					g_nRenderHeight;
	int							g_iValidPoseCount;
	int							g_iValidPoseCount_Last;
	std::string					g_strPoseClasses;			// what classes we saw poses for this frame
	char						g_rDevClassChar[vr::k_unMaxTrackedDeviceCount];	// for each device, a character representing its class

	OgreOpenVR::FramebufferDesc leftEyeDesc;
	OgreOpenVR::FramebufferDesc rightEyeDesc;

#ifdef OGREOPENVR_DX11
	ID3D11DeviceContext *		g_deviceContext = nullptr;
#endif

}

namespace OgreOpenVR
{


	bool initOpenVR(Ogre::SceneManager *sm, Ogre::RenderWindow *rw)
	{
		// Loading OpenVR
		vr::EVRInitError eError = vr::VRInitError_None;
		g_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);
		if (eError != vr::VRInitError_None)
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: VR_Init error");
			g_pHMD = NULL;
			return false;
		}

		/*
		// load render model interface - for cameras, tracked controlers, etc
		g_pRenderModels = (vr::IVRRenderModels *)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
		if (!g_pRenderModels)
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: error getting the render model interface");
			g_pHMD = NULL;
			vr::VR_Shutdown();
			return false;
		}
		*/

		g_strDriver = "No Driver";
		g_strDisplay = "No Display";
		g_strDriver = OgreOpenVR::getTrackedDeviceString(g_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
		g_strDisplay = OgreOpenVR::getTrackedDeviceString(g_pHMD, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

		g_sceneManager = sm;
		g_window = rw;
		g_frameIndex = 0;

		g_pHMD->GetRecommendedRenderTargetSize(&g_nRenderWidth, &g_nRenderHeight);

		// update() gets called before the first updateHMDPos() so we need to make sure this is initialized to something
		g_mat4HMDPose = Ogre::Matrix4::IDENTITY; 

		g_usingGL = (Ogre::Root::getSingleton().getRenderSystem()->getName() == "OpenGL 3+ Rendering Subsystem");

#ifdef OGREOPENVR_GL
		if (g_usingGL)
			gl3wInit();
#endif
#ifdef OGREOPENVR_DX11
		if(!g_usingGL)
			((Ogre::D3D11RenderSystem *)Ogre::Root::getSingleton().getRenderSystem())->_getDevice().get()->GetImmediateContext(&g_deviceContext);
#endif

		if (!initOpenVRCompositor())
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: Failed to initialise OpenVR Compositor");
			return false;
		}

		/* // not currently used
		if (!initOgre(sm))
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: Failed to initialise Ogre");
			return false;
		}
		*/
		
		if( !initOgreTextures())
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: Failed to initialise Ogre textures");
			return false;
		}

		if (!initOpenVRRenderTargets())
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOpenVR: Failed to initialise OpenVR render targets");
			return false;
		}
				
		if (!initOgreCameras())
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOculus: Failed to initialise Ogre Cameras");
			return false;
		}

		if (!initOgreWorkspace())
		{
			Ogre::LogManager::getSingleton().logMessage("OgreOculus: Failed to initialise Ogre compositor workspace");
			return false;
		}

		return true;
	}

	bool initOgre(Ogre::SceneManager *sm)
	{
		return true;
	}



	bool initOpenVRCompositor()
	{
		if (!vr::VRCompositor())
			return false;

		return true;
	}

	// ----------------------------------------------------------------
	// creates the ogre cameras and parent node, and sets the projection matrices
	// ----------------------------------------------------------------
	bool initOgreCameras()
	{
		g_cameraNode = g_sceneManager->getRootSceneNode()->createChildSceneNode();
		g_cameras[0] = g_sceneManager->createCamera("Oculus Left Eye");
		g_cameras[1] = g_sceneManager->createCamera("Oculus Right Eye");

		Ogre::Matrix4 proj;
		for (int i = 0; i < 2; ++i)
		{
			Ogre::Matrix4 camMat4 = getHMDMatrixPoseEye(static_cast<vr::Hmd_Eye>(i));

			// seems like it should work-- the eyes are swapped though?
			// depending how OpenVR does it, this might give us the exact 
			// IPD that the user has set - instead of having to set it in the app
			//Ogre::Vector3 camPos = camMat4.getTrans(); 
			const Ogre::Vector3 camPos(g_IPD * (i - 0.5f), 0, 0);
			Ogre::Quaternion camOrientation = camMat4.extractQuaternion();
			
			g_cameras[i]->setPosition(camPos);
			g_cameras[i]->setNearClipDistance(g_fNearClip);
			g_cameras[i]->setFarClipDistance(g_fFarClip);
			g_cameras[i]->setAutoAspectRatio(true);
			g_cameras[i]->detachFromParent();
			g_cameraNode->attachObject(g_cameras[i]);
			g_cameras[i]->setDirection(0, 0, -1);
			g_cameraNode->setOrientation(camOrientation);

			// get and set the projection matrix for this camera
			proj = getHMDMatrixProjectionEye( static_cast<vr::Hmd_Eye>(i) );
			g_cameras[i]->setCustomProjectionMatrix(true, proj);
		}


		return true;
	}

	// ----------------------------------------------------------------
	// gets the projection matrix for the specified eye
	// ----------------------------------------------------------------
	Ogre::Matrix4 getHMDMatrixProjectionEye(vr::Hmd_Eye nEye)
	{
		if (!g_pHMD)
			return Ogre::Matrix4();

		// there may be a bug in openvr where this is returning a directx style projection matrix regardless of the api specified
		vr::HmdMatrix44_t mat = g_pHMD->GetProjectionMatrix(nEye, g_fNearClip, g_fFarClip, (g_usingGL) ? vr::API_OpenGL : vr::API_DirectX);
				
		// convert to Ogre::Matrix4
		return Ogre::Matrix4(
			mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
			mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
			mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3],
			mat.m[3][0], mat.m[3][1], mat.m[3][2], mat.m[3][3]
		);
	}

	// ----------------------------------------------------------------
	// gets the location of the specified eye
	// ----------------------------------------------------------------
	Ogre::Matrix4 getHMDMatrixPoseEye(vr::Hmd_Eye nEye)
	{
		if (!g_pHMD)
			return Ogre::Matrix4();

		vr::HmdMatrix34_t matEye = g_pHMD->GetEyeToHeadTransform(nEye);	
		Ogre::Matrix4 eyeTransform = convertSteamVRMatrixToOgreMatrix4(matEye);

		return eyeTransform.inverse();
	}

	// ----------------------------------------------------------------
	// figures out the size the render target frame buffers need 
	//  to be and creates them
	// ----------------------------------------------------------------
	bool initOpenVRRenderTargets()
	{
		if (!g_pHMD)
			return false;

		//set up the render targets
		// does nothing at the moment
		createFramebuffer(g_nRenderWidth, g_nRenderHeight, leftEyeDesc);
		createFramebuffer(g_nRenderWidth, g_nRenderHeight, rightEyeDesc);
		
		return true;
	}

	bool createFramebuffer(int nWidth, int nHeight, FramebufferDesc &fbDesc)
	{
		// not needed until we start rendering tracked devices here


		if (g_usingGL)
		{
			/*
			glGenFramebuffers(1, &fbDesc.g_nRenderFramebufferId);
			glBindFramebuffer(GL_FRAMEBUFFER, fbDesc.g_nRenderFramebufferId);

			glGenRenderbuffers(1, &fbDesc.g_nDepthBufferId);
			glBindRenderbuffer(GL_RENDERBUFFER, fbDesc.g_nDepthBufferId);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, nWidth, nHeight);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbDesc.g_nDepthBufferId);
 
			//glGenTextures(1, &fbDesc.g_nRenderTextureId);
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fbDesc.g_nRenderTextureId);
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, nWidth, nHeight, true);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, fbDesc.g_nRenderTextureId, 0);
						
			glGenFramebuffers(1, &fbDesc.g_nResolveFramebufferId);
			glBindFramebuffer(GL_FRAMEBUFFER, fbDesc.g_nResolveFramebufferId);

			//glGenTextures(1, &fbDesc.g_nResolveTextureId);
			glBindTexture(GL_TEXTURE_2D, fbDesc.g_nResolveTextureId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbDesc.g_nResolveTextureId, 0);

			// check FBO status
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE)
			{
				return false;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			
			return true;
			*/
		}
		else // if directX
		{
			/*
			int count = 0;
			for (int i = 0; i < count; ++i)
			{
				ID3D11Texture2D* texture = nullptr;
				ovr_GetTextureSwapChainBufferDX(g_HMD, g_textureSwapChain, i, IID_PPV_ARGS(&texture));
				D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
				rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				((Ogre::D3D11RenderSystem *)Ogre::Root::getSingleton().getRenderSystem())->_getDevice().get()->CreateRenderTargetView(texture, &rtvd, &texRtv[i]);
				texture->Release();

				//fbDesc.g_nRenderFramebufferId = 
			}
			*/
		}
		
		return true;
	}

	// ----------------------------------------------------------------
	// creates the ogre textures for each camera to render to
	// ----------------------------------------------------------------
	bool initOgreTextures()
	{
		g_ogreRenderTexture = Ogre::TextureManager::getSingleton().createManual("Oculus Render Texture", "General", Ogre::TextureType::TEX_TYPE_2D, g_nRenderWidth*2, g_nRenderHeight, 0, Ogre::PixelFormat::PF_R8G8B8A8, Ogre::TextureUsage::TU_RENDERTARGET);
		g_ogreResolveTexture = Ogre::TextureManager::getSingleton().createManual("Oculus Resolve Texture", "General", Ogre::TextureType::TEX_TYPE_2D, g_nRenderWidth*2, g_nRenderHeight, 0, Ogre::PixelFormat::PF_R8G8B8A8, Ogre::TextureUsage::TU_DEFAULT);

		return true;
	}

	// ----------------------------------------------------------------
	// sets up the ogre workspaces and associates the appropriate 
	// camera & render texture to each
	// ----------------------------------------------------------------
	bool initOgreWorkspace()
	{
		const Ogre::IdString workspaceName("OculusStereoWorkspace");

		Ogre::CompositorManager2 *compositorManager = Ogre::Root::getSingleton().getCompositorManager2();
		if (compositorManager->hasWorkspaceDefinition(workspaceName))
		{
			return true;
		}

		compositorManager->createBasicWorkspaceDef(workspaceName, Ogre::ColourValue(0.3f, 0.3f, 0.3f), Ogre::IdString());

		// left eye to the left half of this texture
		g_workspaces[0] = compositorManager->addWorkspace(g_sceneManager, g_ogreRenderTexture->getBuffer()->getRenderTarget(),
			g_cameras[0], workspaceName,
			true, -1,
			Ogre::Vector4(0.0f, 0.0f, 0.5f, 1.0f),
			0x01,
			0x01);

		// right eye to the right
		g_workspaces[1] = compositorManager->addWorkspace(g_sceneManager, g_ogreRenderTexture->getBuffer()->getRenderTarget(),
			g_cameras[1], workspaceName,
			true, -1,
			Ogre::Vector4(0.5f, 0.0f, 0.5f, 1.0f),
			0x02,
			0x02);

		// mirror the left eye to the main window
		compositorManager->addWorkspace(g_sceneManager, g_window,
			g_cameras[0], workspaceName,
			true, -1,
			//Ogre::Vector4(0.0f, 0.0f, 0.5f, 1.0f),
			Ogre::Vector4(0.0f, 0.0f, 1.0f, 1.0f),
			0x01,
			0x01);
		
		/*
		compositorManager->addWorkspace(g_sceneManager, g_window,
			g_cameras[1], workspaceName,
			true, -1,
			Ogre::Vector4(0.5f, 0.0f, 0.5f, 1.0f),
			//Ogre::Vector4(0.5f, 0.0f, 0.5f, 1.0f),
			0x02,
			0x02);
		*/

		return true;
	}

	// ----------------------------------------------------------------
	// gets the current position of all tracked devices
	// ----------------------------------------------------------------
	void updateHMDPos()
	{
		if (!g_pHMD)
			return;

		vr::VRCompositor()->WaitGetPoses(g_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

		g_iValidPoseCount = 0;
		g_strPoseClasses = "";
		for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
		{
			if (g_rTrackedDevicePose[nDevice].bPoseIsValid)
			{
				g_iValidPoseCount++;
				
				g_rmat4DevicePose[nDevice] = convertSteamVRMatrixToOgreMatrix4(g_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);

				if (g_rDevClassChar[nDevice] == 0)
				{
					switch (g_pHMD->GetTrackedDeviceClass(nDevice))
					{
					case vr::TrackedDeviceClass_Controller:        g_rDevClassChar[nDevice] = 'C'; break;
					case vr::TrackedDeviceClass_HMD:               g_rDevClassChar[nDevice] = 'H'; break;
					case vr::TrackedDeviceClass_Invalid:           g_rDevClassChar[nDevice] = 'I'; break;
					case vr::TrackedDeviceClass_Other:             g_rDevClassChar[nDevice] = 'O'; break;
					case vr::TrackedDeviceClass_TrackingReference: g_rDevClassChar[nDevice] = 'T'; break;
					default:                                       g_rDevClassChar[nDevice] = '?'; break;
					}
				}
				g_strPoseClasses += g_rDevClassChar[nDevice];
			}
		}

		if (g_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
		{
			g_mat4HMDPose = g_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
		}
	}

	bool handleInput()
	{
		bool bRet = false;
		
		// Process SteamVR events
		vr::VREvent_t event;
		while (g_pHMD->PollNextEvent(&event, sizeof(event)))
		{
			//ProcessVREvent(event)

			switch (event.eventType)
			{
				case vr::VREvent_TrackedDeviceActivated:
				
					//SetupRenderModelForTrackedDevice(event.trackedDeviceIndex);
					//dprintf("Device %u attached. Setting up render model.\n", event.trackedDeviceIndex);
					break;

				case vr::VREvent_TrackedDeviceDeactivated:
					//dprintf("Device %u detached.\n", event.trackedDeviceIndex);
					break;

				case vr::VREvent_TrackedDeviceUpdated:
					//dprintf("Device %u updated.\n", event.trackedDeviceIndex);
					break;
			}
		
		}

		// Process SteamVR controller state
		for (vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++)
		{
			vr::VRControllerState_t state;
			if (g_pHMD->GetControllerState(unDevice, &state))
			{
				//m_rbShowTrackedDevice[unDevice] = state.ulButtonPressed == 0;
			}
		}

		return bRet;
	}
	
	// ----------------------------------------------------------------
	//  renders the frame
	// ----------------------------------------------------------------
	void update()
	{
		// update the parent camera node's orientation and position with the new tracking data
		g_orientation = g_poseNeutralOrientation.Inverse() * g_mat4HMDPose.extractQuaternion();
		g_cameraNode->setOrientation(g_orientation);
		
		g_position = g_mat4HMDPose.getTrans();
		g_cameraNode->setPosition(g_position - g_poseNeutralPosition);

		/*
		//sensorSampleTime = updateHMDState();
		for (int eye = 0; eye < 2; ++eye)
		{
			g_layer.RenderPose[eye] = g_eyeRenderPose[eye];
			g_layer.SensorSampleTime = sensorSampleTime;
		}
		*/

		Ogre::Root::getSingleton().renderOneFrame();


#ifdef OGREOPENVR_GL
		if (g_usingGL)
		{
			GLuint err;

			bool bFSAA;
			GLuint srcid = ((Ogre::GL3PlusTexture*)g_ogreRenderTexture.get())->getGLID(bFSAA);

			//
			// once we're rendering tracked objects, we'll need to be a little more hands-on with the 
			//  frame buffers.  until then, openVR seems happy enough with the renderbuffer directly

			//GLuint dstid = ((Ogre::GL3PlusTexture*)g_ogreResolveTexture.get())->getGLID(bFSAA);
			/*
			Ogre::GL3PlusFBOManager *fboMan = static_cast<Ogre::GL3PlusFBOManager*>(Ogre::GL3PlusRTTManager::getSingletonPtr());

			glBindFramebuffer(GL_READ_FRAMEBUFFER, fboMan->getTemporaryFBO(0));
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboMan->getTemporaryFBO(1));
			

			glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcid, 0);
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstid, 0);

			glBlitFramebuffer(0, 0, g_nRenderWidth*2, g_nRenderHeight, 0, 0, g_nRenderWidth*2, g_nRenderHeight,
				GL_COLOR_BUFFER_BIT,
				GL_LINEAR);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			*/

			// In OpenGL, the render texture comes out topside-bottomwards.  Not directX though...
			//  need to look into why that is.  until then v1 & v2 values are swapped here to flip it
			//                                       u1    v1    u2    v2
			const vr::VRTextureBounds_t lbounds = { 0.0f, 1.0f, 0.5f, 0.0f };
			const vr::VRTextureBounds_t rbounds = { 0.5f, 1.0f, 1.0f, 0.0f };

			vr::Texture_t stereoTexture = { (void*)srcid, vr::API_OpenGL, vr::ColorSpace_Gamma };

			vr::VRCompositor()->Submit(vr::Eye_Left, &stereoTexture, &lbounds);
			vr::VRCompositor()->Submit(vr::Eye_Right, &stereoTexture, &rbounds);

			glFlush();
			glFinish();
		}

#endif
#ifdef OGREOPENVR_DX11
		if (!g_usingGL)
		{
			// UV coords for each eye's texture are as expected for DirectX
			//                                       u1    v1    u2    v2
			const vr::VRTextureBounds_t lbounds = { 0.0f, 0.0f, 0.5f, 1.0f };
			const vr::VRTextureBounds_t rbounds = { 0.5f, 0.0f, 1.0f, 1.0f };

			vr::Texture_t stereoTexture = { (void*)((Ogre::D3D11Texture*)g_ogreRenderTexture.get())->GetTex2D() , vr::API_DirectX, vr::ColorSpace_Gamma };

			vr::VRCompositor()->Submit(vr::Eye_Left, &stereoTexture, &lbounds);
			vr::VRCompositor()->Submit(vr::Eye_Right, &stereoTexture, &rbounds);
		}
#endif

		g_frameIndex++;

		// update the tracked device positions
		updateHMDPos();		
	}
	
	// ----------------------------------------------------------------
	// converts an OpenVR 3x4 matrix to Ogre::Matrix4
	// ----------------------------------------------------------------
	Ogre::Matrix4 convertSteamVRMatrixToOgreMatrix4(const vr::HmdMatrix34_t &matPose)
	{
		return Ogre::Matrix4(
			matPose.m[0][0], matPose.m[0][1], matPose.m[0][2], matPose.m[0][3],
			matPose.m[1][0], matPose.m[1][1], matPose.m[1][2], matPose.m[1][3],
			matPose.m[2][0], matPose.m[2][1], matPose.m[2][2], matPose.m[2][3],
			           0.0f,            0.0f,            0.0f,            1.0f
		);
	}

	// ----------------------------------------------------------------
	// returns a string describing a tracked device
	// ----------------------------------------------------------------
	std::string getTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError)
	{
		uint32_t unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, NULL, 0, peError);
		if (unRequiredBufferLen == 0)
			return "";

		char *pchBuffer = new char[unRequiredBufferLen];
		unRequiredBufferLen = pHmd->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
		std::string sResult = pchBuffer;
		delete[] pchBuffer;
		return sResult;
	}

	Ogre::Vector3 getPosition()
	{
		return g_position;
	}

	Ogre::Quaternion getOrientation()
	{
		return g_orientation;
	}

	Ogre::Matrix4 getHMDMat4()
	{
		return g_mat4HMDPose;
	}

}
