# ogre-openvr

To initialize, call initOpenVR after initializing the render window and scene manager:
OgreOpenVR::initOpenVR(sceneManager, renderWindow);

Then instead of RenderOneFrame(), call 
OgreOpenVR::update() 
