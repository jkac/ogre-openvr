# ogre-openvr

To initialize, call initOpenVR after initializing the render window and scene manager:
OgreOpenVR::initOpenVR(sceneManager, renderWindow);

Then instead of RenderOneFrame(), call 
OgreOpenVR::update() 

Based off of Kojack's Ogre Oculus example:

http://www.ogre3d.org/forums/viewtopic.php?f=5&t=81627&start=100#p525592

And OpenVR's samples:

https://github.com/ValveSoftware/openvr
