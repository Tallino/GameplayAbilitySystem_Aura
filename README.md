## Project Creation & Architecture

### Character Hierarchy Design
The project establishes a clear inheritance structure for all characters:
- **AuraCharacterBase**: Base class containing shared functionality for all characters
- **AuraCharacter**: Player character implementation
- **AuraEnemy**: Enemy character implementation

**Design Rationale**: This hierarchy allows shared gameplay systems (especially GAS components) to be initialized at the base level while maintaining specialized behavior in derived classes.

### Enemy Variety
Two enemy archetypes demonstrate animation variety:
- Goblin Slingshotters (ranged attackers)
- Goblin Spearmen (melee attackers)

Animations are driven by the CharacterMovement component's velocity, integrated through Blueprint animation graphs.

### Enhanced Input System
**Why Enhanced Input**: Unreal's Enhanced Input system provides flexible, remappable controls with built-in modifiers, essential for modern PC/console games.

**Implementation Approach**:
- Input Mapping Context (IMC) asset stores all input bindings
- Configured in AuraPlayerController for centralized input management
- Input actions use modifiers (e.g., negate for opposing directions: D negates A, axis swizzling for Y-axis)
- Bindings occur in `SetupInputComponent()` using `UInputAction` references, ensuring editor-friendly workflow

### Interface-Based Enemy Interaction
**Design Pattern**: Interface-driven highlighting system for enemy selection

**Why Interfaces**: Allows polymorphic behavior without tight coupling. Any actor can implement `IEnemyInterface` to become targetable, regardless of class hierarchy.

**Cursor Trace System**:
- Executed every tick in PlayerController via `CursorTrace()`
- Performs line trace under mouse cursor
- Checks if hit actor implements `IEnemyInterface` (via cast)
- Maintains `LastActor`/`ThisActor` state to efficiently toggle highlighting only when selection changes

**Benefits**: Clean separation of concerns, extensible to other interactable objects beyond enemies, minimal performance overhead with state caching.

---

## Introduction to the Gameplay Ability System (GAS)

### ASC Ownership Architecture

**Critical Design Decision**: Where to place the Ability System Component (ASC) differs between player and enemy characters:

**Enemies**: ASC and AttributeSet live directly on the character class
- Simple architecture since enemies don't respawn with persistent state
- ASC destroyed with the character on death

**Players**: ASC and AttributeSet live on the PlayerState
- **Why PlayerState**: Preserves abilities and attributes across character respawns/deaths
- PlayerState persists throughout the player's session while the character pawn may be destroyed and recreated
- Enables seamless attribute/ability retention during gameplay events like death or character swaps

### Network Replication Model

**Server Authority**: The server holds the authoritative game state. All clients maintain local copies, but server state takes precedence during conflicts.

**Replication Flow**:
- Attribute changes replicate automatically from server → client
- Client → server communication requires explicit RPCs (Remote Procedure Calls)

### Replication Modes

**Player Characters**: `Mixed` replication mode
- Gameplay Effects (GEs) replicate only to the owning client
- Reduces network bandwidth while ensuring the player sees their own effects
- Other clients don't need full GE details for the player

**Enemy Characters**: `Minimal` replication mode
- GEs are not replicated at all
- Further bandwidth optimization since clients don't need enemy GE details
- Enemy visual feedback handled through other replication mechanisms

### Owner vs Avatar Actor Pattern

GAS separates two critical actor references:

**Owner Actor**: Owns the ASC itself
**Avatar Actor**: The physical character representation

**For Enemies**:
- Owner Actor = Enemy Character
- Avatar Actor = Enemy Character
- Both references point to the same actor

**For Players**:
- Owner Actor = PlayerState (holds the ASC)
- Avatar Actor = Character Pawn (physical representation)
- Separation enables persistence across pawn destruction

### ASC Initialization Timing

**Critical Requirement**: `InitAbilityActorInfo()` must be called after possession to ensure proper network setup.

**Player Characters** (ASC on PlayerState):
- **Server**: Call in `PossessedBy()` - executed when controller possesses the pawn
- **Client**: Call in `OnRep_PlayerState()` - replication notifier triggered when PlayerState replicates
- Must validate PlayerState exists and Controller is set before initialization

**Enemy Characters** (ASC on Pawn):
- **Both Server/Client**: Call in `BeginPlay()`
- Simpler timing since ASC lives directly on the pawn

### Mixed Replication Mode Requirements

**Critical Setup**: When using Mixed replication, the OwnerActor's Owner must be set to the Controller.

**Automatic Cases**:
- Pawns: `PossessedBy()` automatically sets the pawn's Owner to the Controller
- PlayerState: Automatically sets its Owner to the Controller

**Manual Setup Required**: If your OwnerActor is neither a Pawn nor PlayerState (custom setup), you must explicitly call `SetOwner(Controller)` on the OwnerActor.

**Why This Matters**: Mixed replication uses the Owner chain to determine which client should receive replicated GEs. Without proper Owner setup, replication may fail or send data to wrong clients.

---

## Attributes

### Attribute Data Structure

**Core Type**: `FGameplayAttributeData` 
- GAS's fundamental structure for storing attribute values
- Handles both the base value and current value internally
- Provides built-in replication support

### Attribute Replication Setup

**Replication Notification Pattern**:
- Each replicated attribute requires a `OnRep_` function (e.g., `OnRep_Health`)
- Inside the rep notifier, call `GAMEPLAYATTRIBUTE_REPNOTIFY` macro
- **Purpose**: Notifies the AttributeSet that the attribute has replicated, triggering proper GAS callbacks and ensuring system consistency

**Replication Registration**:
- Override `GetLifetimeReplicatedProps()` to mark attributes for replication
- Use `DOREPLIFETIME_CONDITION_NOTIFY` macro for each attribute
- This macro combines replication conditions with rep notify setup in a single call

**Why This Architecture**: GAS needs to track attribute changes for gameplay effects, predictions, and callbacks. The rep notify system ensures these systems stay synchronized across client and server.

### Attribute Accessors

**`ATTRIBUTE_ACCESSORS` Macro**:
- Container macro that generates multiple accessor functions for each attribute
- Creates standardized `InitAttribute()`, `GetAttribute()`, and `SetAttribute()` functions
- **Design Benefit**: Reduces boilerplate code and ensures consistent attribute access patterns across the codebase
- Improves maintainability and reduces errors from manual accessor implementation

---

## RPG Game UI Architecture

### MVC Design Pattern

**Architectural Choice**: The UI system follows Model-View-Controller pattern for clean separation of concerns.

**Component Roles**:
- **View**: Widget (UMG) - Pure presentation layer
- **Controller**: Widget Controller - Data transmission and logic hub
- **Model**: Game data (ASC, AttributeSet, PlayerState, etc.)

**Data Flow**:
- Controller broadcasts data changes → View receives and displays
- View sends user input (button presses) → Controller processes → Updates Model
- Controller can contain algorithmic logic for data transformation

**Dependency Direction**: One-way dependency chain ensures loose coupling
- **Widget → Controller**: Widgets depend on controllers but controllers don't know which specific widgets listen to their broadcasts
- **Controller → Model**: Controllers depend on data sources but Model doesn't know which controllers consume it
- **Benefit**: Highly modular system where components can be swapped without cascading changes

### UI Implementation Structure

**HUD Management**:
- Progress bars (health/mana globes) are Blueprint-based widgets
- AuraHUD class adds widgets to viewport via `InitOverlay()` function
- `InitOverlay()` is called from AuraCharacter during `InitAbilityActorInfo()`, ensuring ASC is ready before UI needs attribute data

**Widget Controller Hierarchy**:
- **AuraWidgetController**: Base class providing common functionality
- **OverlayWidgetController**: Specialized controller for HUD overlay elements

**Controller Dependencies**: All widget controllers maintain a struct containing references to:
- PlayerController
- PlayerState  
- AbilitySystemComponent
- AttributeSet

**Why This Struct**: Centralized access to all gameplay systems the UI needs to observe, passed during initialization.

### Reactive UI Updates with Delegates

**Event-Driven Updates**: UI responds to game state changes through delegate broadcasting.

**Delegate Pattern**:
- Define custom delegate types using GAS macros
- Create delegate instances on the controller (e.g., `OnHealthChanged`, `OnManaChanged`)
- Widgets bind callbacks via `AddLambda()` or `AddDynamic()`
- Controller calls `Broadcast()` when attributes change, triggering all bound callbacks

**Attribute Change Flow**:
1. Gameplay Attribute changes (via Gameplay Effect or direct modification)
2. Controller observes change through GAS callback system
3. Controller broadcasts delegate (e.g., `OnAttributeChanged.Broadcast()`)
4. Widget's Blueprint Event Graph receives callback
5. Blueprint updates visual elements (progress bars, text, etc.)

**Design Rationale**: This reactive pattern decouples the timing and source of attribute changes from UI updates. The controller doesn't need to know implementation details of how the widget displays the data, and new widgets can subscribe without modifying controller code.

---

## Gameplay Effects (GE)

### Core Gameplay Effect Types

**Purpose**: Gameplay Effects are GAS's primary mechanism for modifying attributes through a data-driven approach.

**Duration Types**:
- **Instant**: Applies modifiers once immediately, then expires. Permanently modifies Base value
- **Duration**: Applies for a fixed time period, then automatically expires
- **Infinite**: Persists indefinitely until explicitly removed (by another effect, ability ending, or tag conditions)
- **Periodic**: Executes repeatedly at defined intervals during its active duration. Treated like instant effects at each tick, modifying Base value

**Stacking Behavior**: Effects can accumulate multiple instances based on stacking rules (see Stacking section below).

### Gameplay Effect Data Structures

**FGameplayEffectSpec (GESpec)**:
- Contains the UGameplayEffect class (const data definition)
- Stores effect level and instigator information
- Represents a specific instance of an effect ready to be applied

**FGameplayEffectSpecHandle (GESpecHandle)**:
- Reference wrapper for a GESpec
- **Purpose**: Allows Blueprints to generate a spec once and apply it multiple times to multiple targets
- Improves performance by avoiding redundant spec creation

**FGameplayEffectContext (GEContext)**:
- Data structure tracking instigator and execution metadata (positions, targets, etc.)
- **Extensibility**: Games can subclass this to add custom game-specific information
- Passed throughout effect execution pipeline, ideal for tracking transient execution data

**FGameplayEffectContextHandle (GEContextHandle)**:
- Polymorphic wrapper for FGameplayEffectContext or subclasses
- Ensures proper replication of context data across network

### Effect Configuration

**Modifier Setup** (in GE Blueprint):
- `Gameplay Effect/Modifiers` section defines which attributes to modify
- Targets attributes from the Target's AttributeSet
- Each modifier specifies operation type (Add, Multiply, Override, etc.)

### Stacking Mechanics

**Aggregate by Source**:
- Tracks stacks per individual source
- Different sources maintain independent stack counts
- Example: Enemy A applies 3 poison stacks, Enemy B can apply 3 more independently

**Aggregate by Target**:
- Tracks total stacks received by target regardless of source
- All sources contribute to a single stack count
- Example: All poison effects from any source count toward the same limit

### Effect Application Architecture: AuraEffectActor

**Purpose**: Base class for world actors that apply Gameplay Effects on overlap (potions, hazards, pickups, etc.).

**Policy-Based Design**:
- Effect application/removal behavior controlled via ENUM policies
- Set per-effect-type in editor (e.g., instant heal on overlap, DoT on EndOverlap)
- Policies set to `None/Don't Apply` for irrelevant effect types

**Implementation Flow**:
1. Blueprint handles overlap events (`OnOverlap`/`EndOverlap`)
2. C++ evaluates policy for the triggered event
3. Calls `ApplyEffectToTarget()` with appropriate parameters based on policy

**Design Benefit**: Decouples overlap logic from effect application rules. Same base class supports diverse behaviors (instant pickups, persistent auras, triggered traps) through editor configuration.

### Attribute Change Lifecycle

**PreAttributeChange**:
- **Timing**: Executes before attribute modification is finalized
- **Purpose**: Enforces clamping rules (e.g., Health ≥ 0, Health ≤ MaxHealth)
- **Limitation**: Does NOT permanently modify the underlying modifier values
- Only affects the Current Value returned at that moment
- **Why Repeated Clamping Needed**: Later GE applications recalculate Current Value from all active modifiers, potentially un-clamping the value

**PostGameplayEffectExecute**:
- **Timing**: Triggered after a GE successfully modifies an attribute
- **Purpose**: Central point for reacting to attribute changes with full context

**Data Parameter Benefits**:
- Contains comprehensive Source and Target information
- Includes instigator, ability, tags, magnitude, and more
- Allows complex gameplay logic based on effect context

**FEffectProperties Architecture**:
- **FEffectPropertiesEnhanced**: Wrapper struct with shared pointers distinguishing Source vs Target properties
- **FEffectProperties**: Stores all relevant pointers (Controller, Character, ASC, AttributeSet, etc.)
- **Use Case**: Centralized data extraction from GE execution context
- Final location for clamping Health/Mana to Max values after effect application

### Curve Tables for Level Scaling

**Purpose**: Data-driven scaling of effect magnitudes based on character/ability level.

**Structure**:
- Spreadsheet-like table (rows = levels, columns = effect properties)
- Each cell defines magnitude at that level
- Viewable as graph for visual tuning

**Implementation Examples**:
- **HealingCurve**: Scales healing amount per level
- **ManaCurve**: Scales mana restoration per level

**Design Rationale**: Separates numerical balance from code, enabling designers to iterate on progression curves without programmer involvement. Supports complex scaling formulas through visual graph editing.

---

## Gameplay Tags

### Tag System Fundamentals

**Structure**: Gameplay Tags are hierarchical identifiers built on FNames, using dot notation for parent-child relationships (e.g., `Damage.Fire.Burn`).

**ASC Integration**: The Ability System Component implements `IGameplayTagAssetInterface`, providing query functions:
- `GetOwnedGameplayTags()` - Retrieve all tags
- `HasMatchingGameplayTag()` - Check for specific tag
- `HasAllMatchingGameplayTags()` - Require all specified tags
- `HasAnyMatchingGameplayTags()` - Require at least one tag

**Tag Management**: Gameplay Tag Manager maintains a reference count for each tag instance, tracking how many effects/abilities are currently granting each tag.

### Tags in Gameplay Effects

**Dynamic Tag Granting**: GEs can grant tags to the ASC they're applied to, with duration matching the effect's lifetime.

**Example Use Cases**:
- **Ability Blocking**: Effect grants `Status.Stunned` tag, preventing ability activation
- **Activation Requirements**: Ability requires `Buff.PowerUp` tag to activate
- **Conditional Logic**: Check for `State.InCombat` tag to modify behavior

**Tag Categories**: Tags can represent diverse gameplay concepts:
- Input actions
- Ability identifiers
- Attribute categories
- Damage types
- Buffs/Debuffs
- UI messages
- Generic data markers

### Tag Creation Methods

**Project Settings**: `Edit → Project Settings → Gameplay Tags` (primary method)

**UE 5.3+ GE Editor**: `Gameplay Effect → Components → [+ icon]`, select tag type to add

**Config File**: Directly edit `DefaultGameEngine.ini` (in `Aura/Config/`)

**Data Tables**: Create DT with `GameplayTagTableRow` as row structure for bulk tag management

### Gameplay Effect Tag Categories

**GameplayEffectAssetTag**:
- Tags the GE possesses but does NOT grant to the target actor
- Used for effect identification and filtering

**GrantedTags**:
- Tags applied to the target actor while the effect is active
- Removed when effect expires (for duration/infinite effects)

**Tag Inheritance Structure**:
- **Combined Tags**: Final tag set after inheritance calculations
- **Added Tags**: New tags this effect introduces beyond parent's tags
- **Removed Tags**: Parent tags explicitly excluded in this effect

### Tag Stacking Behavior

**Stacking Type: NONE**:
- Each application creates a new, independent GE instance
- Tags are granted multiple times (separate instances per application)
- Tag count increases with each application

**Stacking Type: NOT NONE** (any aggregation method):
- Multiple applications increase stack count of the same GE instance
- Tags granted only once regardless of stack count
- Tag count remains at 1 despite multiple stacks

**Design Implication**: Choose stacking type based on whether you want tag count to reflect application count.

### Instant Effect Tag Broadcasting

**Challenge**: Instant effects expire immediately, so their tags vanish before they can be observed.

**Solution**: Tag Broadcasting System via `OnGameplayEffectAppliedDelegateToSelf`

**Implementation Flow**:
1. Bind custom `EffectApplied()` function to `OnGameplayEffectAppliedDelegateToSelf` at initialization
2. When instant effect applies, delegate triggers `EffectApplied()`
3. Function extracts AssetTags from the applied effect
4. Broadcasts tags to OverlayWidgetController for UI response

**Why This Works**: Captures effect metadata at the moment of application, before the instant effect disappears.

### Message System Architecture

**Tag-to-UI Pipeline**: Converts Gameplay Effect Asset Tags into on-screen messages.

**Data Flow**:
1. `EffectApplied()` broadcasts AssetTag container via `EffectAssetTags.Broadcast()`
2. OverlayWidgetController receives tags via bound Lambda function
3. For each tag, performs Data Table lookup using parent tag `Message.*` as filter
4. DT row structure defined by `FUIWidgetRow` struct (contains message text, icon, animation data)
5. Broadcasts matching row data to Blueprint (WBP_Overlay)
6. Blueprint creates MessageWidget instance with row's image and text

**Design Benefits**:
- **Data-Driven**: Designers configure messages in DT without code changes
- **Tag Hierarchy**: Using parent tag `Message.*` filters non-message tags, preventing irrelevant lookups
- **Separation of Concerns**: Tag system remains generic while UI layer interprets specific tag meanings

**FUIWidgetRow Purpose**: Bridges gameplay tags to UI presentation data, defining what visual feedback should appear for each message tag.

---

## RPG Attributes

### Attribute Initialization Approaches

**Two Methods Available**:

**Data Table Initialization**:
- Create DT with row structure `AttributeMetaData`
- Define rows as `AuraAttributeSet.AttributeName`
- Assign DT in ASC settings: `AttributeTest → Default Starting Data` (in AuraPlayerState editor)

**Gameplay Effect Initialization** (Preferred Method):
- Self-apply initialization GE at startup (e.g., `GE_AuraPrimaryAttributes`)
- Called in `InitializePrimaryAttribute()` during `InitAbilityActorInfo()`
- **Why Preferred**: More flexible, supports level-based scaling, integrates with GE modifier system

### Attribute-Based Modifiers

**Beyond Scalable Floats**: Attribute-Based modifiers calculate values from other attributes rather than static numbers.

**Configuration Path**: 
`GE → Modifiers → [Index] → Modifier Magnitude → Attribute Based → Backing Attribute`

**Use Case**: Create derived relationships between attributes (e.g., Strength increases Physical Damage, Vigor increases Max Health)

**Critical Consideration**: Modifier order matters significantly, especially with multiplicative operations. GAS applies modifiers sequentially in the order defined.

### Coefficient System for Fine-Tuning

**Purpose**: Allows mathematical manipulation of backing attribute values before final application.

**Three Adjustment Points** (processed in order):
1. **Pre-Multiply Additive Value**: Added to backing attribute before coefficient
2. **Coefficient**: Multiplies the (adjusted) backing attribute value  
3. **Post-Multiply Additive Value**: Added after coefficient multiplication

**Formula**: `FinalValue = (BackingAttribute + PreMultiplyAdditive) × Coefficient + PostMultiplyAdditive`

**Design Benefit**: Enables complex scaling formulas without custom code (e.g., "50% of Strength + 10" becomes Pre=0, Coeff=0.5, Post=10)

### Attribute Categories & Dependencies

**Three-Tier Architecture**:

**Primary Attributes**: Foundation attributes with no dependencies (Strength, Intelligence, Vigor, etc.)
- Initialized first via self-applied GE at startup

**Secondary Attributes**: Derived from primary attributes
- Examples: Armor (from Vigor), Critical Hit Chance (from Dexterity), Max Health (from Vigor)
- Initialized second via self-applied GE after primaries are set
- Use Attribute-Based modifiers pointing to primary attributes

**Vital Attributes**: Core resources depending on secondary attributes
- Health (depends on Max Health)
- Mana (depends on Max Mana)
- Initialized last, ensuring Max values exist before current values are set

**Initialization Order Rationale**: Dependencies must be resolved bottom-up. Initializing vitals before their max values would result in incorrect clamping or ratio calculations.

### Modifier Magnitude Calculation (MMC)

**Purpose**: Enables attribute calculations based on external data beyond just other attributes (e.g., character level, game state).

**Use Case**: Max Health/Mana scaling based on both attribute values AND character level.

**Interface-Based Design**:
- MMC classes depend on `ICombatInterface`, not concrete classes
- `ICombatInterface::GetPlayerLevel()` provides level data
- **Player**: Level stored in PlayerState
- **Enemy**: Level stored in Enemy class
- Both implement interface, maintaining polymorphism

**Implementation Pattern**:
1. Create MMC class inheriting from `UGameplayModMagnitudeCalculation`
2. In constructor: Capture attribute definitions (e.g., Vigor for MaxHealth, Intelligence for MaxMana)
3. Override `CalculateBaseMagnitude_Implementation()`
4. Inside function: Extract backing attribute value and level, apply formula, return result

**Editor Configuration**:
- Set modifier's "Magnitude Calculation Type" to "Custom Calculation Class"
- Assign specific MMC class (e.g., `MMC_MaxHealth`, `MMC_MaxMana`)

**Design Benefits**:
- **Extensible**: New calculation factors (difficulty, buffs) can be added without modifying attribute system
- **Reusable**: Same MMC pattern applies to any complex attribute calculation
- **Polymorphic**: Interface-based approach works across all combat-capable actors

---

9. Attribute Menu
   - We now design our Attribute Menu. We will have WBP_FramedValue, WBP_TextValueRow, and WBP_TextValueButtonRow (where we can add attribute points to our PRIMARY attributes), all contained in WBP_AttributeMenu, and all of them are controlled (brush, width and height settings) with blueprints in their graph whenever we wanted certain parameters to be variables (and non hardcoded). 
   - WBP_Button is a standalone blueprint used for our open/close buttons and our primary attribute "+" sign, and WBP_WideButton is used in the UI interface to open the AttributesMenu. Finally, we open and close the menu from WBP_Overlay; we also use an Event Dispatcher (i.e, blueprint version of delegates), to which the Overlay subscribes, to know when the menu has been destroyed (X button clicked), so to re-enable the WBP_WideButton.
   - So we need to show attribute changes in the UI, but we want to avoid to create a delegate/broadcast FOR EACH attribute, because it is not scalable. So whenever any attribute changes in the model, a generic delegate will be triggered on the WidgetController (FOnAttributeChanged), who will then broadcast different info thanks to a struct we will create that will be based on the attribute's gameplay tag.
   - So the flow is: Widget controller is bound to delegates from the ASC, so when an attribute changes, the widget controller will know about it. In turn, he will take that attribute and try to figure out what gameplay tag it corresponds. Then we can perform a lookup based on the gameplay tag to find its corresponding struct, finally taking all the information from the struct and sending it to the widget: the entity in charge of taking the tag and returning the corresponding struct is called a Data Asset.
   - AuraGameplayTags is a singleton struct where we initialize the attributes tags natively in C++: we will then get the tags from here when we need to send them to the DataAsset. AssetManager is an in-built singleton class from where we inherit and then assign in DefaultEngine.ini file: we then use the new class (AuraAssetManager) to call the initialization on the AuraGameplayTags class. So we create our AttributeInfo class (derived from DataAsset) where we put our struct + an array member variable containing them. Finally, in the editor (in Miscellaneous) we can create a Data Asset file (based on our custom class), where we can manually input the struct information for each index of the array (i.e., each combo of Attribute Tag + Name + Description).
   - We then create our AttributeMenuWidget controller class. Now that we have another widget controller (in addition to the Overlay widget controller), we still construct in AuraHUD just like the overlay, but we want to easily get the widget controller without going through all the classes: in other words, widgets shouldn't go through the GetController function of the HUD to get their controller. So we will use a Blueprint Function Library, where we can use blueprint callable static functions to easily access the controller we want.
   - Therefore, AuraAbilitySystemLibrary class is created where we statically get the widget controllers, through pure blueprint functions, that ultimately gets the controller from the HUD. The HUD constructs the AttributeMenu (in C++), then gets the AttributeMenuWidgetController from the Blueprint Library (who gets it from the HUD, for the reasons said above), and then finally set its controller (all in the blueprint graph).
   - In AttributeMenuWidgetController we create a delegate (in C++) to which the Attribute Rows widget will subscribe. So we find our AttributeInfo struct through tag, broadcast the struct to the widget, who (in blueprint) will set text and value (taken from the broadcast struct). The functions are declared in the base class (WBP_TextValueRow), and called in child class WBP_TextValueButtonRow right after the triggered delegate.
   - To assign the correct name of each attribute to the attribute menu in the UI, we give a name to all rows, make them variables, and set their Attribute Tag in the graph of WBP_AttributeMenu (SetAttributeTags function). Then, in the graph of TextValueRow and TextValueButtonRow, we match the AttributeTag received from the broadcast struct of the controller with the AttributeTag of that specific row: if they match, we set the label.
   - So in the AttributeSet we create a map TagToAttribute, that maps GameplayTags (Attribute Tags) to function pointers (which store the return value of the static getters (accessors) of our attributes). Since the return type of the function pointer is of type TBaseStaticDelegateInstance and a lot of other horrible stuff, we decide to use aliasing to call it simply TStaticFuncPtr (templated version to store whatever func pointer we want). So in the constructor of AuraAttributeSet we add the pairs (AttributeTag to its static getter) to the map, then in the widget controller we loop in the map, based on the AttributeTag key we get the Attribute variable, from which we GetNumericalValue, finally adding the value to the AttributeInfo struct and then broadcasting it to the widget.
   - Finally, to be sure that attributes change in the UI when they change at runtime, we loop (in BindCallbacksToDependencies) into our map binding to each Attribute a lambda function (thanks to in-built function GetGameplayAttributeValueChangeDelegate, so trigger the lambda everytime the attribute changes). The lambda is obviously the usual broadcast of the correct AttributeInfo struct to the widgets.
10. Gameplay Abilities
    - Gameplay abilities are actions/skills that an actor can perform, but rather than implementing the action with a simple function, a GA can be an instanced object, running asynchronously. This means it can be activated at some point in time, and run multistage tasks, that may or may not span across periods of time. Ability Tasks perform asynchronous work during a gameplay ability execution. They can affect execution flow by broadcasting delegates.
    - The ASC must be granted the ability, and when this happens, a GameplayAbilitySpec is created (it defines the detail pertaining the GA). Abilities are granted on server, and when this happens, the Spec is replicated to the client, so they can activate it from there. Once activated, GAs are said to be active. Then they either End, or Cancelled. Finally, they have cost and cooldown, and they can run asynchronously (multiple active at the same time).
    - In AuraCharacter's PossessedBy we call a function to set the abilities. This calls a function in AuraCharacterBase where we call the ASC passing an array of abilities we set through BP. The ASC will then loop in this array and finally add + activate the abilities. Our base class for all GAs is AuraGameplayAbility.
    - Abilities Tags are the tags that the abilities has. All the rest of the settings are explained in the comments in the details panel of the GA. What NOT TO USE: Replication Policy (Useless, as GAs are already replicated automatically); Server Respects Remote Ability Cancellation (it means the client can decide on the server, not a good idea); Replicate Input Directly (so possibly calling an RPC at each frame, highly non-performative and therefore discouraged).
    - So we want to bind inputs to GAs. Before we bound inputs directly to the ASC, but it was too rigid. Now with Enhanced input, we bind them via the Input Mapping Context. We will have a data driven approach, using a Data Asset that is going to contain input actions, each linked with gameplay tags. So we want to assign various tags to our GAs, in order change input-to-ability mappings at runtime.
    - AuraInputConfig is our Data Asset based class where we store arrays of structs composed of gameplay tags and input actions. Input actions are created in blueprint and assigned to the first 4 numbered keys + left mouse click + right mouse click in our IMC_AuraContext file. Tags are created natively in C++ in our GameplayTags singleton file. Finally, they are paired via blueprint in the array mentioned before.
    - AuraInputComponent instead is where we declare a template function in charge of binding the future gameplay functions to the InputActions, passing a tag as an input parameter. Each Input Action will be able to bind up to 3 different function: for Pressed, Released and Held. In AuraPlayerController we call the BindActions, passing in the InputConfig, and bind to each of our actions (as for now, 4 keys + LMB + RMB) three different functions for pressed, held and released. We can indeed also see printed out the tags associated to the actions.
    - Finally, from the PlayerController we call the functions in the ASC that then activates the abilities with exact tag match. Remember that when (in function AddCharacterAbilities) we loop through the StartupAbilities array (set in BP on AuraCharacter), adding their tag to the DynamicAbilityTags array, then finally give the ability to the ASC, we can then find the abilities in a prebuilt array called GetActivatableAbilities(). Indeed, this is where we finally take the Specs and activate them based on their tag (calling, in the ASC, the final prebuilt function AbilitySpecInputPressed);
    - To move our character we will use AddMovementInput (as it works also in multiplayer). But since normal pathfinding algorithms would result in abrupted change of movements in case of an obstacle lying in the middle of the path, we will use a spline to approximate the path with a curve and make it smoother. First of all, when we click the first time (inside AbilityInputTagPressed), we check if we are using the LMB and if we are targeting something, setting a boolean). Then, inside AbilityInputTagHeld, if we are not using LMB it means we are using an ability for sure, else, if we are using LMB and not targeting, then again, we are using an ability for sure. Finally, if we are using LMB and NOT targeting, then we cache the direction under the cursor, and add movement input to that direction.
    - In AbilityInputTagReleased instead, we do the same check as before, but if we are using LMB and NOT targeting, when we release, we want to autorun there: we use FindPathToLocationSynchronously(), taking the pathPoints of the result and adding them to our spline component. In the tick, we check if we are autorunning, and if yes, we add movement in the direction of the closes spline point to us. If we get close to the point under the AutoRunAcceptanceRadius, we stop.
    - We now want to create projectiles and our base class will be AuraProjectile. All children of this class will have a USphereComponent and a UProjectileMovementComponent (we set initial speeds already). In BeginPlay, we AddDynamic (delegate) to function OnSphereOverlap. AuraProjectileSpell instead is our GA class (inheriting from GameplayAbility.h), that will override and call ActivateAbility: if we are the server, we spawn an AuraProjectile in deferred way (i.e., 2-steps: so later we can GE spec to cause damage in between) with a blueprint callable function. Small note: we spawn it at the tip of the staff, thanks to an FVector location we keep on ICombatInterface (that Aura inherits from), giving its name based on the bone name in the editor.
11. Ability Tasks
    - Ability tasks are the workers that a GA employs to do tasks and instantaneous/periodical stuff. An example is PlayMontageAndWait (available both in BP and C++), that unites animations and conditional events (to which we can bind several delegates, such as onCompleted, onCancelled etc.). In order to spawn the fire bolt at the right moment, we create a MontageEvent (derived from AnimNotify), and call it at the exact moment in the montage (when the staff is fully stretched). In the MontageEvent, we override ReceiveNotify so to send a GameplayEvent (with tag FireBolt, added in project settings) to the actor (FireBolt GA) when we get notified. Finally, in the GA event graph, we WaitGameplayEvent (with FireBolt tag), and then spawn the fire bolt right after (after EventReceived connection pin) with the above-mentioned blueprint callable function (SpawnProjectile).
    - We use an ability task (TargetDataUnderMouse) to send the location data under our mouse cursor at the time of firing the bolt (we get the player controller from the ability task, then the HitResult, then we broadcast the location through delegates). In BP, we actually use the delegate as execution pin, and we see it works in singleplayer but not in multiplayer: indeed the location of the mouse cursor is not replicated up to the server from other clients.
    - So first, our purpose here is to send the data (cursor hit result) from the client to the server, so the server can use this data to spawn the fire bolt (we want him to be responsible for doing it). To send the data to the server we will use RPCs, but if Activate gets called on the server before the valid data gets there (through RPC), then we would be activating with invalid data, causing problems. GAS has a TargetData instance, based on parent struct FGameplayAbilityTargetData. To send data up to the server, we will use ServerSetReplicatedTarget(). Once data reached the server, it takes its TargetSet (a delegate, FAbilityTargetDataSetDelegate) and broadcasts that delegate. On the server a map is maintained (AbilityTargetDataMap, which maps AbilitySpecs to TargetData). So finally, as soon as we get Activate function called, we send target data to the server, and if Activate gets called first, we will have Activate bound to TargetSetData, so we will broadcast the data right after. But, if the replicated target data reaches the server first, then Activate gets called, its too late: the broadcast has already happened before we bound to it, we missed it. So in that case, we can use CallReplicatedTargetDataDelegateIfSet(): it's going to provide a safeguard to us, as it will force the broadcast of that delegate again in case the replicated target data gets there first and the delegate is broadcast before the server can bind to the target set delegate.
    - So in Activate() we have different flows based on whether we are client or server. If we are client, we send/replicate the data under the hit result (using FGameplayAbilityTargetData_SingleTargetHit) to the server (packaged up in a handle called FGameplayAbilityTargetDataHandle), but still nonetheless broadcasting the data right after for local purposes. Instead, if we are the server, and we need to listen/receive data, we bind to our callback. If the data has not arrived yet, no problem we wait, and we will broadcast the delegate when we receive it. Otherwise, if the data already arrived (before we bound to the delegate), our callback will never execute, to address this case we call CallReplicatedTargetDataDelegatesIfSet(), which will first check if the data have already been received, if so, it will re-broadcast the delegate, so we can get our callback finally executed.
    - In C++ we therefore set the rotation of the projectile direction (mouse data received - spawn location of the projectile), and then pass it to SpawnProjectile in BP. We also added the possibility of using the shift button to move and fire simultaneously and moreover to avoid running towards the enemy if we missclick slightly next to him when trying to fire. Finally, we use Motion Warping to make Aura rotate towards the fire direction, setting the Motion Warping component in AuraCharacter, setting it in the track of the firebolt Animation Montage, and finally setting its warp location in the BP, calling a function on casted ICombatInterface (to avoid making the spell dependent on the character).
