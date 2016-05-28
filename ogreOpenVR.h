#ifndef OGREOPENVR_H
#define OGREOPENVR_H

#include "ogreprerequisites.h"

namespace OgreOpenVR
{
	struct FramebufferDesc
	{
		uint32_t g_nDepthBufferId;
		uint32_t g_nRenderTextureId;
		uint32_t g_nRenderFramebufferId;
		uint32_t g_nResolveTextureId;
		uint32_t g_nResolveFramebufferId;
	};

	bool initOpenVR(Ogre::SceneManager *sm, Ogre::RenderWindow *rw);
	bool initOpenVRCompositor();
	bool initOpenVRRenderTargets();

	bool initOgre(Ogre::SceneManager *sm);
	bool initOgreCameras();
	bool initOgreTextures();
	bool initOgreWorkspace();

	bool createFramebuffer(int nWidth, int nHeight, FramebufferDesc &fbDesc);

	bool handleInput();
	void updateHMDPos();
	void update();

	//void setPosition(Ogre::Vector3 pos);
	Ogre::Vector3 getPosition();
	Ogre::Quaternion getOrientation();
	Ogre::Matrix4 getHMDMat4();
	//void resetPose();

	// helper functions
	Ogre::Matrix4 convertSteamVRMatrixToOgreMatrix4(const vr::HmdMatrix34_t &matPose);
	std::string getTrackedDeviceString(vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL);
	Ogre::Matrix4 getHMDMatrixProjectionEye(vr::Hmd_Eye nEye);
	Ogre::Matrix4 getHMDMatrixPoseEye(vr::Hmd_Eye nEye);
}






#endif
