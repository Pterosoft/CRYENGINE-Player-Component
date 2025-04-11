# CRYENGINE Player Component

This will create a player character entity inside of CRYENGINE. The entity contains simple cylinder shaped physics for the character, advanced animation component, and Player Component. In the player component you will be able to set the camera position, maximum and minimum rotation, animation names for each movement, walk speed, sprint speed, jump height and mouse rotation speed (mouse sensitivity). These is also a camera entity that will allow you to customize the camera view.

## Using the component
First you need to open the .cryproject file - which is the project file for CRYENGINE. It is usually called Game.cryproject. Search for "plugins": and add following line  
{ "guid": "", "type": "EType::Native", "path": "bin/win_x64/Game.dll" },  
if you are adding the plugin to the last position - don't use ,. The .dll file is called Game.dll for now but that will change in the future. After you do this add the .dll file to bin/win_x64 folder as used in the plugin reference. Then you can open the editor/sandbox. You can find the entity in Components->Player->Player Controller

## Animation Loading
The animations must be prepared in mannequin editor. Load everything from Animation Database to Default Fragment Name. Set Default Fragment Name to your idle animation and check Animation Driven Motion. In the collumn Physics set Mass(pre-scale) to arround 80 and make sure Weight Type is set to Mass. The value we inserted equals to 80kg.

## Physics
In Character Controller tab set Mass again to 80 to get the correct weight and make sure the cylinder is matching your character height and radius. It is better for the cylinder to be a little bit bigget then the player character then otherwise. Use Collider Radius and Collider Height for that.

## Camera
In the camera tab make sure that Active is checked. If you want you can change the FOV. I recommend keeping the rest of the settings as they are.

## Basic Settings
Player Walk Speed will set up the speed of the character when walking, Player Run Speed when running/sprinting and Player Jump Height will set up how tall the jump will be. Player Rotation Speed adjust the rotation speed/mouse sensitivity. I recomment keeping this to a low number - below 0. Camera Offset Standing and Camera Offset Crouching will set the position of the camera. The X value is usually 0. The Y value should have a slightly positive value so that the camera is placed in from of the character and Z value should reflect the height of your character so that the camera is in the poisition of the head. In Crouching - the Z value should be 30 - 40% smaller then while standing. Capsule Height Standing and Crouching represents the height of the physics capsule/cylinder. Here you should copy the Z value from the Camera Offset settings. Camera Pitch Max and Mix limit the camera rotation when looking up or down. Max should be negative value and Min should be positive value - somewhere between 1 - 2, depending on your character model.

## Animations
In the player entity you can also asign the locomotion animations for your player character. My goal was to create this component re-usable so that you can use it with different character without hard-coding the animation into C++. Here you have multiple animation - all of those must be filled in order for the player character to work correctly. You need to type in Fragment Name from your mannequin setup. If you're not sure you can click on Default Fragment Node which will display a list of all existing fragments. Note that this is case sensitive and all of the names must match.

#### Default Settings
Player Walk Speed: 2  
Player Run Speed: 5  
Player Jump Height: 3  
Player Rotation Speed: 0.002
Camera Offset Standing: X:0; Y:0.175; Z:1.7  
Canera Offset Crouching: X:0; Y:0.175; Z:1  
Capsule Height Standing: 1.7  
Capsule Height Crouching: 1  
Capsule Ground Offset: 0.2  
Camera Pitch Max: -1.1  
Camera Pitch Min: 1.5  

## Flowgraph Nodes
In addition to the component editing I added some flowgraph nodes as well so that some of the functionalities can be used during gameplay and are not static. You can find the nodes by opening Flowgraph and then go to Player Component folder.

### Change Input Bind Node
This node allows you to change the input bind - meaning change the controls of the player. To see all of the inputs available go to INPUT-FLAGS.md where you can find a list of inputs that you can use. Make sure to delete eKI_. So if you want to change the forward button to ESC add Escape (from eKI_Escape) into New Key. Then input action name moveforward into Action Name and trigger the node. The component supports PC, Xbox, Playstation and Oculus controls. As before everything is case sensitive so keep that in mind when working with this node. Below is the list of input actions that you can add to Action Name.

#### Input Actions
moveforward - Forward  
moveback - Back  
moveleft - Left  
moveright - Right  
sprint - Sprint/Run  
jump - Jump  
crouch - Crouch  

### Play Custom Animation
This node will allow you to play custom animation. When this is triggered it will override any currently ongoing animation. Again you will need to type in the Fragment Name from the mannequin editor. You cal also select if the animation will be motion driven.
