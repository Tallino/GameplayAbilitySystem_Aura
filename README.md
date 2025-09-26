1. Introduction
	- Introduction to the Course


2. Project Creation
	- Setup inheritance: AuraCharacterBase is base class, AuraCharacter and AuraEnemy are inherited
	- 2 different type of enemy (with different animations): Goblin slingshots and Goblin Spearmen
	- Animations take Velocity from CharacterMovement component and sets it to the Animation (in blueprint)
	- Enhanced Input (IMC file) used in AuraPlayerController. It is stored in IMC file (Input Mapping Context). D keybind is negated A, same for Y axis (Swizzle) etc.
	- Move function is bound to Enhanced Input Component at startup time (SetupInputComponent). Binding to MoveAction (of type UInputAction), then used in the editor with the same name
	- EnemyInterface abstract class is given to all enemies to inherit from. From the Player Controller, at each tick, we call CursorTrace(), checking the HitResult under the cursor trace. If the actor hit 	is inheriting the interface (checked with a cast), then we highlight the actor (or unhighlight) with a LastActor/ThisActor logic


3. Intro to the Gameplay Ability System
	- Enemies have the Ability System Component and AttributeSet directly on the character class, but for the player it is kept on the PlayerState (so it is not lost when the player dies)
	- Each client has its own game state/copy, same for the server, but server's copy is the authorative one. Replication of attribute changes happens only from server to client. If client want to replicate to server, he must use RPCs
	- Replication Mode on player will be of type Mixed (GEs will be replicated to owning client only), while on enemies will be of type Minimal (GEs won't be replicated).
	- ASCs have Owner Actor and Avatar Actor. So for enemies, Owner and Avatar are the same Enemy Character. For players, the Owner Actor is the Player State, while the Avatar Actor will be the character
	- InitAbilityActorInfo must be done after possession: for players ASC lives on PlayerState, so make sure that PlayerState is valid and Controller has been set. Server calls it inside PossessedBy function, client inside OnRep_PlayerState function (a rep notifier i.e. a function being called as a result of something being replicated); for enemies ASC lives on pawn, so call it inside BeginPlay.
	- For Mixed Replication mode, the OwnerActor's Owner must be the Controller. For pawns, this is set automatically in PossessedBy(). The PlayerState's Owner is automatically set to the Controller. So if your OwnerActor is not the PlayerState, and you used Mixed Replication mode, you must call SetOwner() on the OwnerActor to set its owner to the Controller


4. Attributes
	- FGameplayAttributeData is the type for attributes. Inside OnRep_Health (notifier), we call the macros in charge of notifying the AS (GAMEPLAYATTRIBUTE_REPNOTIFY). To mark a variable as replicated, the function in charge is GetLifetimeReplicatedProps. Inside here, we call DOREPLIFETIME_CONDITION_NOTIFY macro.
	- ATTRIBUTE_ACCESSORS is a container macro for defining several accessor macros, for easily initting, setting and getting the attributes (with Init/Get/Set<AttributeName>)


5. RPG Game UI
	- UI uses MVC design pattern. The View is the widget, the controller is Widget controller and the Model is the data. The controller is in charge of transmitting data to the view, but also of transmitting button presses from the view to the data. It can also have algorithmic logic inside. One way dependency: so the widget's depend on the controller (controller doesnt need to know which widget are receving data broadcast to them), and the controller depends on the model (controller doesn't need to know the widget that the system has).
	- All the (globe) progress bars (health and mana) are handled via blueprint: the HUD class adds these widget to the viewport, in the InitOverlay function (called in AuraCharacter during InitAbilityActorInfo). 
	- AuraWidgetController is the mother class, OverlayWidgetController is inherited. All widget controllers have a struct containing pointers to PlayerController, PlayerState, AbilitySystemComponent and AttributeSet.
	- To change Health/Mana in the view we use callbacks (delegates). The Broadcast function triggers the delegate, and calls all functions that have been bound to that delegate (through AddLambda). Remember that the MACRO is necessary to define the custom TYPE of your delegate, and then you define it's instances.
	- When we call OnAttributeChanged.Broadcast as a callback for when a GameplayAttribute changes, the trigger for the actual change of the view is made in blueprint (event graph of the WBP).


6. Gameplay Effects
	- GE change attributes through modifiers. They can have be instant, (have a) duration, or infinite, and they can stack. 


7. Gameplay Tags
	- 

