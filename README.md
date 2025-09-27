1. Introduction
	- Introduction to the Course.


2. Project Creation
	- Setup inheritance: AuraCharacterBase is base class, AuraCharacter and AuraEnemy are inherited.
	- 2 different type of enemy (with different animations): Goblin slingshots and Goblin Spearmen.
	- Animations take Velocity from CharacterMovement component and sets it to the Animation (in blueprint).
	- Enhanced Input (IMC file) used in AuraPlayerController. It is stored in IMC file (Input Mapping Context). D keybind is negated A, same for Y axis (Swizzle) etc.
	- Move function is bound to Enhanced Input Component at startup time (SetupInputComponent). Binding to MoveAction (of type UInputAction), then used in the editor with the same name.
	- EnemyInterface abstract class is given to all enemies to inherit from. From the Player Controller, at each tick, we call CursorTrace(), checking the HitResult under the cursor trace. If the actor hit is inheriting the interface (checked with a cast), then we highlight the actor (or unhighlight) with a LastActor/ThisActor logic.


3. Intro to the Gameplay Ability System
	- Enemies have the Ability System Component and AttributeSet directly on the character class, but for the player it is kept on the PlayerState (so it is not lost when the player dies).
	- Each client has its own game state/copy, same for the server, but server's copy is the authorative one. Replication of attribute changes happens only from server to client. If client want to replicate to server, he must use RPCs.
	- Replication Mode on player will be of type Mixed (GEs will be replicated to owning client only), while on enemies will be of type Minimal (GEs won't be replicated).
	- ASCs have Owner Actor and Avatar Actor. So for enemies, Owner and Avatar are the same Enemy Character. For players, the Owner Actor is the Player State, while the Avatar Actor will be the character.
	- InitAbilityActorInfo must be done after possession: for players ASC lives on PlayerState, so make sure that PlayerState is valid and Controller has been set. Server calls it inside PossessedBy function, client inside OnRep_PlayerState function (a rep notifier i.e. a function being called as a result of something being replicated); for enemies ASC lives on pawn, so call it inside BeginPlay.
	- For Mixed Replication mode, the OwnerActor's Owner must be the Controller. For pawns, this is set automatically in PossessedBy(). The PlayerState's Owner is automatically set to the Controller. So if your OwnerActor is not the PlayerState, and you used Mixed Replication mode, you must call SetOwner() on the OwnerActor to set its owner to the Controller.


4. Attributes
	- FGameplayAttributeData is the type for attributes. Inside OnRep_Health (notifier), we call the macros in charge of notifying the AS (GAMEPLAYATTRIBUTE_REPNOTIFY). To mark a variable as replicated, the function in charge is GetLifetimeReplicatedProps. Inside here, we call DOREPLIFETIME_CONDITION_NOTIFY macro.
	- ATTRIBUTE_ACCESSORS is a container macro for defining several accessor macros, for easily initting, setting and getting the attributes (with Init/Get/Set<AttributeName>).


5. RPG Game UI
	- UI uses MVC design pattern. The View is the widget, the controller is Widget controller and the Model is the data. The controller is in charge of transmitting data to the view, but also of transmitting button presses from the view to the data. It can also have algorithmic logic inside. One way dependency: so the widget's depend on the controller (controller doesnt need to know which widget are receving data broadcast to them), and the controller depends on the model (controller doesn't need to know the widget that the system has).
	- All the (globe) progress bars (health and mana) are handled via blueprint: the HUD class adds these widget to the viewport, in the InitOverlay function (called in AuraCharacter during InitAbilityActorInfo). 
	- AuraWidgetController is the mother class, OverlayWidgetController is inherited. All widget controllers have a struct containing pointers to PlayerController, PlayerState, AbilitySystemComponent and AttributeSet.
	- To change Health/Mana in the view we use callbacks (delegates). The Broadcast function triggers the delegate, and calls all functions that have been bound to that delegate (through AddLambda). Remember that the MACRO is necessary to define the custom TYPE of your delegate, and then you define it's instances.
	- When we call OnAttributeChanged.Broadcast as a callback for when a GameplayAttribute changes, the trigger for the actual change of the view is made in blueprint (event graph of the WBP).


6. Gameplay Effects
	- Gameplay Effects (GE) change attributes through modifiers. They can be instant, (have a) duration, infinite, or periodic, and they can stack. Instant applies its modifiers immediately, once, then expires; Infinite applies and persists indefinitely until explicitly removed (by another effect, an ability ending, or a tag condition); Duration applies for a fixed amount of time, then automatically expires. Periodic executes its effect repeatedly at intervals (ticks) during its active time.
    - GESpec (specification) tells us what UGameplayEffect (const data), what level and who instigated. GESpecHandle allows blueprints to generate a GameplayEffectSpec once and then reference it by handle, to apply it multiple times/multiple targets.
    - GEContext is a Data structure that stores an instigator and related data, such as positions and targets. Games can subclass this structure and add game-specific information. It is passed throughout effect execution so it is a great place to track transient information about an execution. GEContextHandle wraps a FGameplayEffectContext or subclass, to allow it to be polymorphic and replicate properly.
    - In GE blueprint in the editor, section Gameplay Effect/Modifiers is where you select what attribute to modify from the AttributeSet of the Target
    - Periodic GEs are treated like instant (permanently change the Base value).
    - Stacking: Aggregate by Source (how many stacks that source specifically already applied in general): so a difference source could apply other stacks, and the counter would start from 0 again; Aggregate by Target (how many stacks has the target received, independently if they are from difference sources)
	- AuraEffectActor is a class attached to any actor able to apply a (Gameplay) effect to other actors overlapping with it. Policies (coded as ENUMS) decide how the AuraEffectActor will apply or remove the effects, and are set into the editor. They are kept to None/Don't Apply for the type of effects that are not the ones we mean the actor to apply. We call OnOverlap/EndOverlap in blueprint, and trigger the case (in C++) based on the policy, which in turn calls ApplyEffectToTarget with the correct parameters.
	- PreAttributeChange enforces clamping to avoid Health going in negative etc., and is triggered by changes to Attributes (Attribute Accessors, Gameplay Effects etc.), but executed before that attribute actually changes. It does NOT permanently change the modifier, just the value returned from querying the modifier. Later operations recalculate the Current Value from all modifiers, so we might need to clamp again.
	- PostGameplayEffectExecute is an overriden function triggered after a gameplay effect changes an attribute, and it is incredibly useful as its paramater Data contains a lot of useful properties from both Source and Target of the GE. We indeed use this function to gather this infromation and store into the FEffectPropertiesEnhanced struct. This wrapper struct has different shared pointers (based on Source/Target Properties) to the FEffectProperties struct, which is where we finally store pointers to all the info we ultimately care about. Finally, we clamp again the Health and Mana (to MaxHealth and MaxMana).
	- Curve Tables (CT) is like an excel spreadsheet, where we decide how to scale the floats that regulate the intensity of how much a GE applies, based on the object level. We can then view this grid of level/effect intensity (row/column) in a graph view. We do this for both HealingCurve and ManaCurve.

7. Gameplay Tags
	-  

