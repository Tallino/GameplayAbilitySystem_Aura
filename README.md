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

## Attribute Menu

### Widget Architecture

**Widget Hierarchy**:
- **WBP_FramedValue**: Visual container for attribute displays
- **WBP_TextValueRow**: Read-only attribute display
- **WBP_TextValueButtonRow**: Interactive row with "+" button for spending attribute points on primary attributes
- **WBP_AttributeMenu**: Parent container holding all attribute rows
- **WBP_Button**: Reusable button component for open/close actions and primary attribute increment buttons
- **WBP_WideButton**: UI button to open the AttributeMenu

**Design Choice**: Parameterize styling (brush, dimensions) through Blueprint-exposed variables rather than hardcoding, enabling rapid iteration without recompilation.

**Menu State Management**:
- Menu opened/closed from WBP_Overlay
- Event Dispatcher notifies Overlay when menu is destroyed (X button clicked)
- Overlay re-enables WBP_WideButton upon receiving destruction event

### Scalable Attribute Broadcasting Architecture

**Challenge**: Avoid creating individual delegates for each attribute (non-scalable for 20+ attributes).

**Solution**: Generic attribute change system using Gameplay Tags as identifiers.

**Data Flow**:
1. **Model Layer**: Attribute changes in AttributeSet (via GE or direct modification)
2. **Controller Layer**: Widget controller binds to ASC's attribute change delegates
3. **Tag Resolution**: Controller identifies which Gameplay Tag corresponds to changed attribute
4. **Data Lookup**: Tag used to query Data Asset for attribute metadata struct
5. **Broadcasting**: Generic `FOnAttributeChanged` delegate broadcasts struct to all subscribing widgets
6. **Widget Layer**: Widgets filter broadcasts by matching their AttributeTag to received struct's tag

**Key Benefit**: One delegate handles all attributes. New attributes require zero new delegate infrastructure.

### Data Asset Pattern for Attribute Metadata

**Purpose**: Decouple attribute metadata (names, descriptions, icons) from code.

**AuraGameplayTags Singleton**:
- Native C++ struct storing all attribute tags
- Centralized tag repository accessed throughout codebase
- Initialized via AssetManager pattern

**AssetManager Integration**:
- Create `AuraAssetManager` inheriting from engine's `UAssetManager` singleton
- Register in `DefaultEngine.ini` to ensure early initialization
- Triggers `AuraGameplayTags` initialization at engine startup

**AttributeInfo Data Asset**:
- Custom class derived from `UDataAsset`
- Contains struct definition with: Tag, Name, Description, Icon, Value
- Exposes array of these structs for editor population
- **Editor Workflow**: Create DA instance in Miscellaneous, manually configure each attribute's metadata

**Design Rationale**: Designers can modify attribute presentation without touching code. Supports localization and rapid content iteration.

### Widget Controller Access Pattern

**Problem**: Multiple widget controllers (Overlay, AttributeMenu, etc.) need consistent access pattern.

**Anti-Pattern**: Direct `GetController()` calls through HUD create tight coupling and verbose Blueprint graphs.

**Solution**: Blueprint Function Library for centralized controller access.

**AuraAbilitySystemLibrary**:
- Static functions marked `BlueprintCallable` and `BlueprintPure`
- Encapsulates HUD retrieval and controller construction logic
- **Usage**: Widgets call library functions directly, library handles HUD interaction

**Construction Flow**:
1. HUD constructs AttributeMenu widget controller (C++)
2. Blueprint calls library function to retrieve controller
3. Library fetches from HUD internally
4. Blueprint sets controller on widget

**Benefits**: Simplifies Blueprint graphs, centralizes controller creation logic, reduces coupling between widgets and HUD.

### Attribute Display Update System

**Broadcasting Pattern**:
- AttributeMenuWidgetController defines delegate for attribute updates (C++)
- WBP_TextValueRow and WBP_TextValueButtonRow subscribe to delegate
- Controller finds AttributeInfo struct by tag, broadcasts to all subscribers
- Base class (WBP_TextValueRow) declares update functions
- Child class (WBP_TextValueButtonRow) calls inherited functions when delegate triggers

**Tag Matching in Widgets**:
- Each row widget assigned a specific AttributeTag (set in WBP_AttributeMenu's `SetAttributeTags()`)
- Rows receive all broadcasts but only update when broadcast tag matches their assigned tag
- **Performance Note**: Acceptable overhead since attribute changes are infrequent

### Tag-to-Attribute Mapping System

**Challenge**: Convert Gameplay Tag to actual attribute value at runtime.

**Solution**: `TMap<FGameplayTag, TStaticFuncPtr>` in AttributeSet.

**TStaticFuncPtr Type Alias**:
- Hides complex template syntax: `TBaseStaticDelegateInstance<...>`
- Templated alias stores function pointers to attribute getter functions
- **Purpose**: Makes code readable while maintaining type safety

**Map Construction** (in AuraAttributeSet constructor):
- Populate map with pairs: `(AttributeTag → Static Getter Function Pointer)`
- Example: `(Tags.Attributes.Primary.Strength → FAttributeSet::GetStrengthAttribute)`

**Runtime Lookup Flow**:
1. Widget controller iterates through TagToAttribute map
2. For each tag, calls stored function pointer to retrieve attribute
3. Calls `GetNumericValue()` on returned attribute
4. Populates AttributeInfo struct with tag + value
5. Broadcasts struct to widgets

### Reactive Attribute Updates

**BindCallbacksToDependencies Implementation**:
- Iterate through TagToAttribute map
- For each attribute, bind lambda to `GetGameplayAttributeValueChangeDelegate(Attribute)`
- Lambda captures necessary context (tag, controller)
- When attribute changes, delegate triggers lambda
- Lambda constructs updated AttributeInfo struct and broadcasts to widgets

**Design Elegance**: Single binding loop handles all attributes reactively. Adding new attributes requires only updating the TagToAttribute map; binding system automatically includes them.

**Why This Architecture Works**: Combines GAS's built-in attribute change delegates with data-driven tag system, creating a scalable solution that grows linearly with attribute count rather than exponentially with widget-attribute combinations.

---

## Gameplay Abilities

### Gameplay Ability Fundamentals

**Conceptual Shift**: Unlike traditional function-based actions, Gameplay Abilities (GA) are instanced objects capable of asynchronous, multi-stage execution.

**Key Characteristics**:
- Can run across multiple frames/time periods
- Support complex, stateful behavior
- Execute asynchronously (multiple abilities active simultaneously)

**Ability Tasks**: Specialized objects performing asynchronous work during ability execution. Control execution flow through delegate broadcasting.

### Ability Lifecycle

**Granting Phase**:
- Server grants ability to ASC
- Creates `GameplayAbilitySpec` containing ability configuration and state
- Spec automatically replicates to owning client
- **Why Specs**: Separates ability definition (class) from runtime instance data (spec)

**Execution States**:
1. **Granted**: Ability available in ASC but inactive
2. **Active**: Ability currently executing
3. **Ended**: Ability completed successfully
4. **Cancelled**: Ability interrupted before completion

**Resource Management**:
- Cost: Resources consumed on activation (mana, stamina, etc.)
- Cooldown: Time before ability can be activated again

### Ability Initialization Architecture

**Granting Flow**:
1. `AuraCharacter::PossessedBy()` calls ability setup function
2. Calls into `AuraCharacterBase` passing Blueprint-configured ability array
3. ASC loops through array, granting and optionally activating abilities

**Base Class**: `AuraGameplayAbility` - Foundation for all project-specific abilities

### Gameplay Ability Configuration

**Ability Tags**: Tags the ability possesses for identification and filtering.

**Settings to AVOID**:
- **Replication Policy**: Redundant (GAs already replicate automatically)
- **Server Respects Remote Ability Cancellation**: Allows client authority over server decisions (breaks authority model)
- **Replicate Input Directly**: Triggers RPC per frame (severe performance cost)

**Design Principle**: Rely on GAS's built-in replication rather than manual configuration. Manual settings often introduce bugs or performance issues.

### Data-Driven Input Binding System

**Evolution**: Previous rigid direct-to-ASC binding replaced with flexible tag-based system.

**Architecture Goal**: Runtime-remappable input → ability associations using Gameplay Tags.

**AuraInputConfig Data Asset**:
- Custom `UDataAsset` subclass storing input configuration
- Contains array of structs: `(GameplayTag, UInputAction*)`
- **Input Actions**: Created in Blueprint, assigned to keys (1-4) and mouse buttons in IMC
- **Tags**: Defined natively in C++ GameplayTags singleton
- **Pairing**: Configured in Blueprint via Data Asset instance

**Benefits**: Designers can change key bindings, ability-input mappings, and input contexts without code changes.

### Enhanced Input Integration

**AuraInputComponent**:
- Custom input component with templated binding function
- Supports three callback types per action: **Pressed**, **Held**, **Released**
- Accepts Gameplay Tag as parameter to associate input with ability

**Binding Flow** (in AuraPlayerController):
1. Call `BindActions()` with InputConfig Data Asset
2. For each InputAction in config (4 keys + LMB + RMB):
   - Bind Pressed callback
   - Bind Held callback
   - Bind Released callback
3. Each callback receives associated Gameplay Tag

**Runtime Activation**:
- PlayerController functions call ASC with tag
- ASC searches `GetActivatableAbilities()` for matching spec
- Calls `AbilitySpecInputPressed()` on matching ability
- **Tag Matching**: Exact match required between input tag and ability's tags

**Why Three Callbacks**: Enables complex input handling (charge attacks on hold, quick-cast on press, cancel on release).

### Movement System with Pathfinding

**Design Choice**: `AddMovementInput()` for multiplayer compatibility.

**Challenge**: Standard pathfinding produces jarring direction changes when encountering obstacles.

**Solution**: Spline-based path smoothing for natural character movement.

**Input State Logic**:

**AbilityInputTagPressed** (initial click):
- Check if LMB + targeting enemy
- Set boolean flag for target state

**AbilityInputTagHeld** (button held):
- If not LMB → activate ability
- If LMB + targeting → activate ability (attack enemy)
- If LMB + not targeting → cache cursor direction, add movement input

**AbilityInputTagReleased** (button released):
- If LMB + not targeting → initiate autorun
- Call `FindPathToLocationSynchronously()` to get path points
- Populate spline component with path points

**Autorun Execution** (in Tick):
- Find the closest spline point to character
- Add movement input toward that point
- Stop when within `AutoRunAcceptanceRadius` of destination

**Design Rationale**: Separates click-to-move from ability activation, prevents accidental ability usage during movement, provides smooth pathfinding without custom navigation mesh modifications.

### Projectile System

**AuraProjectile Base Class**:
- `USphereComponent`: Collision detection
- `UProjectileMovementComponent`: Automatic movement with configured initial speed
- `BeginPlay()`: Binds `OnSphereOverlap` delegate with `AddDynamic`

**AuraProjectileSpell Gameplay Ability**:
- Inherits from `UGameplayAbility`
- Overrides `ActivateAbility()`

**Server-Side Spawning**:
- Authority check ensures server executes spawn
- **Deferred Spawning Pattern**: Two-step spawn process
  1. Spawn projectile with incomplete initialization
  2. Configure Gameplay Effect spec for damage
  3. Finish spawning
- **Why Deferred**: Allows GE configuration between spawn and initialization

**Spawn Location**: Staff tip position via `ICombatInterface::GetCombatSocketLocation()`
- Interface method returns socket location by bone name
- Bone name configured in editor per character
- **Polymorphism Benefit**: Different characters can have different socket names while using same ability code

---
      
## Ability Tasks

### Ability Task Fundamentals

**Purpose**: Ability Tasks are worker objects that execute asynchronous/periodic operations during Gameplay Ability execution.

**Example**: `PlayMontageAndWait` - Integrates animation playback with event-driven execution flow
- Available in both Blueprint and C++
- Provides delegate callbacks: OnCompleted, OnCancelled, OnInterrupted, etc.
- **Design Benefit**: Couples animation timing with ability logic without manual frame-by-frame checking

### Animation-Driven Ability Timing

**Challenge**: Spawn projectile at precise animation moment (staff fully extended).

**Solution**: Animation Notify system integrated with Gameplay Events.

**Implementation Flow**:
1. Create custom `AnimNotify` subclass (`MontageEvent`)
2. Override `ReceiveNotify()` to send Gameplay Event with `FireBolt` tag
3. Place notify at exact montage frame
4. In GA Blueprint: `WaitGameplayEvent` node listens for `FireBolt` tag
5. On `EventReceived` pin: Execute `SpawnProjectile()`

**Design Rationale**: Decouples ability logic from animation frame numbers. Animators control timing; programmers define behavior. Animation changes don't require code updates.

### Client-Server Target Data Replication

**Challenge**: Mouse cursor location is client-side data not automatically replicated to server.

**Problem**: Server must spawn projectiles (authority), but lacks client's cursor position.

**Naive Approach Fails**: Simply sending cursor data via RPC introduces race condition:
- If `Activate()` called before RPC arrives → spawns with invalid data
- If RPC arrives before `Activate()` called → delegate broadcast missed

### GAS Target Data System

**FGameplayAbilityTargetData**: Parent struct for various target data types.

**FGameplayAbilityTargetData_SingleTargetHit**: Encapsulates single hit result (location, actor, component, etc.).

**FGameplayAbilityTargetDataHandle**: Container for target data, enables replication.

**ServerSetReplicatedTarget()**: Built-in RPC for sending target data client → server.

**AbilityTargetDataMap**: Server-side map `(AbilitySpec → TargetData)`
- Stores target data as it arrives via RPC
- Persists data across ability activation timing

**FAbilityTargetDataSetDelegate**: Delegate broadcast when target data is set on server.

### Race Condition Solution

**Two Timing Scenarios**:

**Scenario A**: `Activate()` called before target data arrives
1. Server calls `Activate()`
2. Binds to `TargetSetDelegate`
3. Target data arrives via RPC
4. Delegate broadcasts → bound callback executes ✓

**Scenario B**: Target data arrives before `Activate()` called
1. Target data arrives via RPC
2. Delegate broadcasts (no listeners yet)
3. Server calls `Activate()`
4. Binds to delegate (too late, broadcast already happened) ✗

**CallReplicatedTargetDataDelegatesIfSet() Safeguard**:
- Checks if target data already exists in `AbilityTargetDataMap`
- If present: Re-broadcasts delegate immediately
- Ensures callback executes regardless of arrival timing

### Activate() Implementation Pattern

**Client Path**:
1. Package cursor hit result into `FGameplayAbilityTargetData_SingleTargetHit`
2. Wrap in `FGameplayAbilityTargetDataHandle`
3. Send to server via `ServerSetReplicatedTarget()`
4. Broadcast locally for client-side prediction/feedback

**Server Path**:
1. Bind callback to `TargetSetDelegate`
2. Call `CallReplicatedTargetDataDelegatesIfSet()`
   - If data already arrived: Immediately re-broadcasts delegate
   - If data not arrived: Waits for RPC, broadcasts when received

**Why This Works**: Covers both timing scenarios with single safeguard call. No race condition possible.

### Projectile Direction Calculation

**Implementation** (in C++):
- Calculate rotation: `MouseHitLocation - ProjectileSpawnLocation`
- Pass rotation to Blueprint's `SpawnProjectile()` function
- **Separation of Concerns**: C++ handles calculation, Blueprint handles visual spawning setup

### Enhanced Movement Controls

**Shift + Click**: Move and fire simultaneously without stopping.

**Targeting Tolerance**: Prevents accidental movement when clicking near (but not on) enemies during attack.

**Design Goal**: Reduce player frustration from input ambiguity in fast-paced combat.

### Motion Warping Integration

**Purpose**: Smoothly rotate character toward projectile fire direction during cast animation.

**Setup**:
1. Add `MotionWarpingComponent` to AuraCharacter
2. Configure warping in FireBolt animation montage track
3. Set warp target location in Blueprint via `ICombatInterface`

**Interface Usage**: Keeps spell code character-agnostic. Different characters can override warp behavior without modifying spell.

### Impact Effects & Audio

**Visual/Audio Elements**:
- Impact effect and sound on projectile collision
- Looping hissing sound during projectile flight (in animation montage)

**Replication Timing Challenge**: Sphere overlap may occur before/after client replication.

**Solution**: Authority checks ensure effects play on clients regardless of replication timing.
```cpp
if (HasAuthority()) {
    // Server plays effects
}
// Client also plays effects independently
```

### Custom Collision Configuration

**Projectile Collision Channel**: Custom channel in Project Settings
- Default: Ignore all
- Block: Pawns, Enemies
- Ignore: Pickups, world static geometry (for visual effects)

**Actor Setup**: Manually configure overlap response per actor type, enable "Generate Overlap Events."

**Code Integration**: Define collision channel constant in `Aura.h` using `#define` for easy reference.

### Damage Application Architecture

**Gameplay Effect Assignment**:
1. Create Damage GE in editor
2. Assign to `SpecHandle` in `AuraProjectileSpell` Blueprint
3. Pass to `AuraProjectile` during deferred spawn
4. **Timing**: Between `SpawnActorDeferred()` and `FinishSpawning()`

**Application Point**: Server-side only, on sphere overlap with enemy.

**Temporary Visualization**: Self-applied attribute initialization on enemies allows instant health reduction testing (subtract 10 from Health).

### Enemy Health Bar System

**Widget Architecture**:
- **WBP_ProgressBar**: Base class for all progress bars
- Enemy-specific health bar inherits from base

**Controller Pattern Reuse**: Enemy class acts as its own widget controller.

**Delegate Reuse**: "Borrow" attribute change delegate from `OverlayWidgetController`.

**Binding Flow** (in Enemy BeginPlay):
- Bind attribute change delegate
- Lambda captures widget reference
- Broadcasts new health value on change
- Widget's `SetPercent()` receives broadcast

**Design Benefit**: Avoids duplicating delegate infrastructure. Single attribute system serves both player UI and enemy health bars.

### Ghost Bar (Delayed Health Visual)

**Purpose**: Shows previous health value with delay, creating visual lag effect.

**Implementation** (Blueprint only):
- Interpolate health value every tick with delay timer
- Reset timer on each damage event
- Hide bar after parameterized inactivity period
- Initialize hidden in `PreConstruct`

**Design Choice**: Pure Blueprint for rapid visual tuning without recompilation. Artists can adjust timing/interpolation curves independently.

---

## RPG Character Classes

### Character Class System Architecture

**Design Goal**: Support multiple character archetypes with distinct starting attributes and abilities.

**Class Types**:
- **Warrior**: Melee-focused, high Vigor
- **Ranger**: Ranged physical, high Dexterity  
- **Elementalist**: Magic caster (e.g., Aura)

**Data-Driven Approach**: Separate class definitions from code using Data Assets and Curve Tables.

### Core Components

**ECharacterClass Enum**: Defines available character classes (Warrior, Ranger, Elementalist).

**CharacterClassInfo Data Asset**: Central repository for all class-specific data.

**Curve Tables**: Per-class progression curves for primary attributes across levels.

**Gameplay Effects**: Class-specific GEs for initializing primary attributes; shared GEs for secondary/vital attributes.

**Ability Grants**: Each class defines starting abilities in Data Asset.

**Common Properties**: Shared across all enemies (death mechanics, common abilities, etc.).

### CharacterClassInfo Structure

**FCharacterClassDefaultInfo Struct**:
- Contains default primary attribute values for a specific class
- Defines starting point before level scaling

**Class Member Variables**:
- Secondary attribute initialization data (shared across classes)
- Vital attribute initialization data (shared across classes)
- `TMap<ECharacterClass, FCharacterClassDefaultInfo>` - Maps class enum to default info
- Lookup function for convenient map access

**Data Asset Blueprint**: `DA_CharacterClassInfo` inherits from `CharacterClassInfo` class, configured in editor.

### Gameplay Effect Organization

**Class-Specific Primary Attribute GEs**:
- `GE_Primary_Warrior`
- `GE_Primary_Ranger`
- `GE_Primary_Elementalist`
- Stored in Enemy folder (Aura has personal copy)

**Shared Attribute GEs**:
- Secondary attributes GE (same for all classes)
- Vital attributes GE (same for all classes)
- **Design Rationale**: Primary attributes differentiate classes; secondary/vital use shared formulas

**Data Asset Assignment**: All GEs referenced in `DA_CharacterClassInfo` for centralized configuration.

### Curve Table Design for Level Scaling

**Structure**: One Curve Table per class
- Each CT contains curves for all primary attributes of that class
- Each curve defines attribute value at each level

**Curve Creation Methods**:

**Manual Entry**:
1. Right-click → "Add Key"
2. Set (Level, Attribute Value) pairs
3. Apply auto-interpolation for smooth curves

**Import from File**:
- CSV/JSON import for bulk data entry
- Export to CSV/JSON for external editing/version control
- **Benefit**: Designers can use Excel for balance tuning, import results

**GE Configuration**:
- Add modifiers to `GE_Primary_[Class]`
- Select "Override" magnitude calculation
- Reference appropriate class Curve Table
- Choose specific curve within CT

**Why Curve Tables**: Non-linear progression (early levels gain more, late levels diminish) without hardcoded formulas. Balance iteration happens in spreadsheet, not code.

### Initialization System

**AuraAbilitySystemLibrary Static Function**:
- `InitializeDefaultAttributes(Class, Level)` - Centralized initialization entry point
- Self-applies appropriate GEs based on class and level
- Standard GAS flow: Create ContextHandle → Create SpecHandle → Apply to ASC

**Execution Point**: Called in `AuraEnemy::InitializeDefaultAttributes()` during `InitAbilityActorInfo()`.

**Data Source**: `CharacterClassInfo` reference stored in **GameModeBase**
- **Why GameMode**: Global, server-authoritative, accessible from any actor
- Library fetches Data Asset from GameMode during initialization

**Enemy Integration**: `AuraEnemy` overrides `InitializeDefaultAttributes()`, calls library function with its class and level.

**Design Benefits**:
- **Centralized**: Single initialization path for all characters
- **Data-Driven**: Class balance changes require zero code changes
- **Scalable**: Adding new classes requires only Data Asset configuration
- **Authority**: GameMode storage ensures server controls class definitions

**Attribute Application Order** (maintained automatically):
1. Primary attributes (class-specific via Curve Table)
2. Secondary attributes (derived from primaries, shared GE)
3. Vital attributes (derived from secondaries, shared GE)

---

## Damage

### Meta Attributes Concept

**Purpose**: Temporary calculation placeholders for complex damage formulas.

**Why Meta Attributes**:
- Receive intermediate calculations (armor reduction, critical hits, blocks, level scaling)
- NOT replicated (performance optimization for transient data)
- Final value transfers to actual replicated attribute after all calculations complete

**IncomingDamage Meta Attribute**:
- Accumulates all damage modifiers before application
- Subtracted from Health in `PostGameplayEffectExecute()`
- `GE_Damage` modifies IncomingDamage, not Health directly

**Design Benefit**: Separates damage calculation complexity from Health attribute. Enables multiple damage modifiers to compose before final application.

### SetByCaller Damage System

**Problem**: Hardcoded damage values prevent dynamic scaling and ability-specific damage.

**Solution**: SetByCaller magnitude system - runtime damage assignment via Gameplay Tags.

**Implementation Flow**:
1. Create Damage Gameplay Tag
2. In GE editor: Configure modifier magnitude as "Set By Caller"
3. In ability code: Call `AssignTagSetByCallerMagnitude(SpecHandle, DamageTag, DamageValue)`
4. SpecHandle carries tag-value pairs throughout execution

**Scalable Float Integration**:
- GE_Damage uses Curve Table for level-based damage scaling
- Call `GetValueAtLevel(GetAbilityLevel())` to retrieve scaled damage
- Pass result to `AssignTagSetByCallerMagnitude()`

**Benefits**: 
- One GE supports all abilities (damage value varies per ability)
- Level scaling handled via Curve Table
- Multiple damage types via different tags on same spec

### Hit Reaction System

**GE_HitReact**: Applies HitReact tag when damage lands.

**Tag Count Delegate Binding**: Monitors HitReact tag addition/removal.

**Reaction Flow**:
1. Damage applied → HitReact tag granted
2. Delegate triggers → Enemy movement stops
3. Animation plays via `ICombatInterface::GetHitReactMontage()`
4. Blueprint executes `PlayMontageAndWait` in `GA_HitReact`

**Interface Design**: `ICombatInterface` stores montage reference
- Aura and Enemy each define class-specific montages
- Ability code remains character-agnostic

**Ability Assignment**:
- HitReact ability stored in `CommonAbilities` array (CharacterClassInfo Data Asset)
- Granted at startup via `AuraAbilitySystemLibrary` static function
- Activated in AttributeSet when IncomingDamage is non-fatal

**Why Common Abilities**: Shared behaviors (hit react, death) don't need per-class definitions.

### Death System

**Trigger Point**: AttributeSet's fatal damage check (IncomingDamage ≥ Health).

**ICombatInterface Death Functions**: Pure virtual functions implemented by Character and Enemy.

**Death Sequence** (not animation-based):
1. Detach weapon from character
2. Enable ragdoll physics
3. Trigger dissolve effect

**Dissolve Effect**:
- Two timelines: Character dissolve + Weapon dissolve
- Implemented in Blueprint using existing dissolve material
- Timelines control material parameters over time

**Design Choice**: Blueprint timelines allow VFX artists to tune dissolve timing/curves without code changes.

### Damage Text Widget System

**Widget Architecture**:
- Create damage number widget Blueprint
- Animate appearance/fade in widget designer
- Play animation on spawn

**DamageTextComponent**:
- Custom component with `SetDamageText()` function
- Blueprint implementation calls `UpdateDamageText()` on widget
- **Separation**: C++ defines interface, Blueprint handles visual presentation

**Spawning Flow**:
1. `AuraAttributeSet` detects incoming damage
2. Calls `SetDamageText()` with damage value and target character
3. Forwards to **PlayerController** (not AttributeSet directly)
4. PlayerController spawns widget at target location
5. Attaches widget to target character
6. Sets damage text value

**Why PlayerController**: 
- Client-side UI responsibility
- Ensures damage numbers appear for local player even with network latency
- Separates gameplay logic (AttributeSet) from presentation (UI)

### Gameplay Effect Execution Calculation

**UGameplayEffectExecutionCalculation**: Advanced damage calculation class enabling complex, multi-attribute formulas.

**Advantages over Simple Modifiers**:
- Modify multiple attributes simultaneously
- Implement arbitrary programmer logic (conditionals, random chance, complex math)
- Capture source and target attributes for comparative calculations

**Snapshotting Decision**: NOT used on target attributes
- Target values captured at effect application time, not ability activation
- Ensures current armor/resistances apply, not values from cast start

**SetByCaller Integration**: Still uses SetByCaller magnitude for base damage input.

### Execution Calculation Implementation

**Class Setup**:
- Inherit from `UGameplayEffectExecutionCalculation`
- Override `Execute_Implementation()`

**Captured Data Access**:
- Source ASC, Actor
- Target ASC, Actor
- Owning GE Spec

**Attribute Capture Pattern**:
- Define static struct with attribute capture macros
- Use `AttemptCalculateCapturedAttributeMagnitude()` to retrieve captured values
- Enables compile-time validation of captured attributes

**Output Application**:
- Call `AddOutputModifier(OutExecutionOutput, Attribute, Value)` for each modified attribute
- **GE Configuration**: Assign execution class in GE_Damage under "Execution" section of Modifiers

**Example Use**: Modify IncomingDamage based on target's Armor
- **Implication**: Arbitrarily complex formulas across any captured attributes

### Complex Damage Formula Implementation

**Base Damage**: Retrieved via `GetSetByCallerMagnitude()` from spec (set in AuraProjectileSpell).

**Block Chance**:
1. Capture target's BlockChance attribute
2. Generate random float [0.0, 1.0]
3. If random < BlockChance: Halve damage
4. **Design**: Probabilistic mitigation encouraging defensive builds

**Armor Penetration**:
- Capture source's ArmorPenetration and target's Armor
- Calculate effective armor: `Armor × (1 - ArmorPenetration%)`
- Reduce damage based on effective armor
- **Design**: ArmorPen soft-counters high-armor targets without nullifying armor entirely

**Coefficient Scaling**:
- Store coefficient Curve Tables in CharacterClassInfo Data Asset
- Access via `AuraAbilitySystemLibrary` static getter
- Retrieve coefficient based on Source/Target level
- Apply to armor, penetration, block calculations

**Critical Hit System**:
- Capture source's CriticalHitChance, CriticalHitDamage
- Capture target's CriticalHitResistance
- **Effective Crit Chance**: `CritChance - CritResistance`
- If random < Effective Crit Chance: `Damage × (1 + CritDamage%)`

**Design Benefits**:
- **Data-Driven**: Balance tuning via Curve Tables
- **Level Scaling**: Coefficients adjust with character progression
- **Extensible**: New damage modifiers integrate without refactoring existing formulas
- **Stat Interplay**: Offensive stats counterbalance defensive stats naturally

---

## Advanced Damage Techniques

### Custom Gameplay Effect Context

**Limitation**: Base `FGameplayEffectContext` lacks storage for custom combat data (critical hits, blocks, damage sources).

**Solution**: Subclass `FGameplayEffectContext` to add project-specific information.

**AuraGameplayEffectContext** (in AuraAbilityTypes):
- Inherits from `FGameplayEffectContext`
- Adds boolean fields: `bIsCriticalHit`, `bIsBlockedHit`
- Implements getters/setters for custom data
- Overrides `GetScriptStruct()` - Returns custom struct type for reflection
- Overrides `NetSerialize()` - Custom network serialization
- Overrides `Duplicate()` - Proper copying for instanced contexts

### Network Serialization with Bit Packing

**NetSerialize() Purpose**: Convert struct data to bits for efficient network transmission.

**Bit Packing Strategy**:
- Use single `uint8 RepBits` to store multiple boolean flags
- Each bit represents one boolean's replication status
- **Benefits**: 8 booleans in 1 byte vs. 8 bytes for naive approach

**Serialization (Saving)**:
```cpp
RepBits |= (bIsCriticalHit ? 1 : 0) << 0;  // Bit 0
RepBits |= (bIsBlockedHit ? 1 : 0) << 1;   // Bit 1
```

**Deserialization (Loading)**:
```cpp
bIsCriticalHit = (RepBits & (1 << 0)) != 0;  // Check bit 0
bIsBlockedHit = (RepBits & (1 << 1)) != 0;   // Check bit 1
```

**Archive Operator Overload**: `<<` operator works bidirectionally
- Saving: Writes to archive
- Loading: Reads from archive

**Design Elegance**: Single serialization function handles both directions based on archive mode.

### Struct Traits Configuration

**TStructOpsTypeTraits Enums** (set to true):
- **WithNetSerializer**: Enables custom `NetSerialize()` function
- **WithCopy**: Ensures proper copying behavior for instanced data

**Why Required**: Informs Unreal's serialization system about custom requirements, preventing default (incorrect) serialization.

### Registering Custom Context Globally

**AuraAbilitySystemGlobals**:
- Inherit from `UAbilitySystemGlobals`
- Override `AllocGameplayEffectContext()`
- Return instance of `FAuraGameplayEffectContext`

**Integration Point**: `CreateEffectContext()` calls `AllocGameplayEffectContext()` internally.

**DefaultGame.ini Configuration**: Register custom globals class so engine uses it project-wide.

**Design Impact**: All GE contexts throughout the project automatically use custom type without per-context configuration.

### Data Flow: Calculation to UI

**Setting Context Data** (in ExecCalc_Damage):
- Calculate critical hit result → Call setter on context
- Calculate block result → Call setter on context
- Context stored in GE Spec

**Retrieving Context Data** (in AttributeSet::PostGameplayEffectExecute):
- Extract context from executed GE
- Call getters for `bIsCriticalHit`, `bIsBlockedHit`
- Pass booleans to DamageTextComponent

**UI Presentation** (in WBP_DamageText Blueprint):
- Receive boolean combination (4 states: normal, crit, block, crit+block)
- Branch to appropriate text color
- Display corresponding message for special cases

**AuraAbilitySystemBlueprintLibrary**: Provides Blueprint-accessible getters/setters wrapping context functions.

**Design Benefit**: Data flows seamlessly from calculation → context → AttributeSet → UI without losing information across network boundaries.

### Damage Type System

**Architecture Evolution**: Move from single damage value to typed damage system.

**AuraDamageGameplayAbility**: New base class for damage-dealing abilities
- `TMap<FGameplayTag, float> DamageTypes` - Damage type → magnitude mapping
- Populated in editor per-ability
- `AuraProjectileSpell` inherits from this class

**Damage Type Tags** (in GameplayTags singleton):
- Fire
- Lightning
- Arcane
- Physical

**SetByCaller Integration**:
- Loop through `DamageTypes` map
- For each type: Call `AssignTagSetByCallerMagnitude(Spec, TypeTag, Value)`
- Single spec carries multiple typed damage values

**Calculation Retrieval** (in ExecCalc_Damage):
- Loop through damage type tags
- For each: Call `GetSetByCallerMagnitude(TypeTag)`
- Accumulate total damage from all types

**Design Benefits**:
- Abilities deal multiple damage types simultaneously (e.g., Fire + Physical hybrid attack)
- Per-type balancing without separate GEs
- Extensible: New types require only tag creation

### Resistance System

**Resistance Attributes**: Match each damage type
- FireResistance
- LightningResistance
- ArcaneResistance  
- PhysicalResistance

**Attribute Implementation**:
- Standard `FGameplayAttributeData` with rep notifiers
- Added to `TagsToAttributes` map
- Initialized via `GE_SecondaryAttributes` (derived from Resilience primary attribute)
- Displayed in WBP_AttributeMenu

**DamageTypesToResistances Map** (in GameplayTags singleton):
- `TMap<FGameplayTag, FGameplayTag>` linking damage type to corresponding resistance
- Example: `Damage.Fire` → `Attributes.Resistance.Fire`

**ExecCalc_Damage Integration**:
- Create local map: `TMap<FGameplayTag, FGameplayAttributeCaptureDefinition>` linking tags to attribute captures
- Capture all resistance attributes from target
- For each damage type in incoming damage:
  1. Look up corresponding resistance tag in `DamageTypesToResistances`
  2. Retrieve resistance attribute value via map lookup
  3. Reduce damage: `TypeDamage × (1 - Resistance%)`

**Calculation Flow**:
```
Base Fire Damage: 100
Target Fire Resistance: 30%
Final Fire Damage: 100 × (1 - 0.30) = 70
```

**Design Benefits**:
- **Granular Defense**: Build characters resistant to specific damage types
- **Strategic Depth**: Encourages diverse damage type usage in abilities
- **Scalable**: Adding new damage type requires: tag creation, attribute creation, map entry
- **Data-Driven**: Resistance values initialized via GE and Curve Tables like other attributes

---

## Enemy AI

### Behavior Tree Architecture

**Design Philosophy**: Modular, data-driven AI using Behavior Trees (BTs) for easy swapping between enemy archetypes.

**Core Components**:
- **Behavior Trees**: Define decision logic, assigned per-enemy in Blueprint
- **AI Controllers**: Own BT Components and manage execution
- **Blackboards**: Shared data storage for BT nodes and services

**Initialization Flow** (AuraEnemy):
- Override `PossessedBy()`
- Call BT/Blackboard initialization on `AuraAIController`
- `AuraAIController` set as default AI Controller class in Blueprint

**Design Benefit**: Different enemy types share the same base classes but use different BTs, enabling diverse behaviors without code duplication.

### Behavior Tree Services

**FindNearestPlayer Service**:
- Custom service (Blueprint inheriting from `BTService`)
- Attached to selector node in BT
- Overrides `Event Receive Tick AI` - Service tick function

**Service Execution**: Ticks periodically while node is active, updating Blackboard data.

### Blackboard Data Storage

**Purpose**: Centralized variable storage accessible across all BT nodes and services.

**Key Types**: Support various data types (Actors, Vectors, Floats, Bools, etc.)

**FindNearestPlayer Implementation**:
1. Determine target tag based on controlled pawn
   - If AI controls Enemy → search for `Player` tag
   - If AI controls Player → search for `Enemy` tag
2. Find all actors with target tag
3. Calculate distances to each
4. Store nearest actor and distance in Blackboard keys

**Tag System**: Simple FName tags (not Gameplay Tags)
- Enemies: Tagged `Enemy`
- Player: Tagged `Player`
- **Why FNames**: Lightweight identification for spatial queries

### Behavior Tree Decorators

**Purpose**: Conditional execution - control which BT branch executes based on Blackboard state.

**Decorator Examples**:
- **Is Ranged Attacker**: Checks enemy archetype
- **Distance to Target**: Validates proximity for attack
- **Line of Sight**: Confirms visibility to target

**Configuration**: Decorators read Blackboard keys (set in C++), evaluate conditions in real-time.

**Branch Selection Logic**:
- Ranged attack path (if ranged + has line of sight)
- Melee attack path (if melee + close enough)
- Move to target path (fallback)

**BTT_GoAroundTarget Task**:
- Calls `GetRandomLocationInNavigableRadius()` around target
- Assigns result to `TargetToFollow` Blackboard key
- **Purpose**: Creates organic movement patterns between attacks, prevents static positioning

**Design Benefit**: Decorators provide readable, designer-friendly conditional logic without code.

### Environment Query System (EQS)

**Problem**: Ranged enemies need intelligent positioning - attack from line of sight, reposition when blocked.

**Solution**: EQS generates and evaluates potential positions dynamically.

**EQS Workflow**:
1. **Generate Items**: Create test positions around target (grid, circle, etc.)
2. **Run Tests**: Filter and score positions based on criteria
3. **Return Best**: Select optimal position for AI action

### EQS Configuration

**EQSTestingPawn**: Debug visualization tool
- Assign Environment Query asset
- Visualizes generated positions and test results in-editor

**Item Generation**:
- Pattern types: Grid, Circle, Donut, Custom
- Parameters: Grid size, spacing between positions, radius

**Trace Test (Visibility)**:
- Test type: Line-of-sight check
- Filter mode: **Filter Only** (remove positions without LoS)
- Context: `EQS_PlayerContext` (returns all Aura actors)
- **Result**: Only positions with unobstructed view to player remain

**Distance Test**:
- Test type: Distance to querier (AI pawn)
- Scoring mode: **Score Only** (rank remaining positions)
- Scoring factor: **-1** (inverse - closer = higher score)
- **Result**: Closest viable position scores highest

**Test Composition**: Filtering + Scoring enables "closest position with line of sight" queries in single EQ.

### BT Integration with EQS

**Ranged Attacker Selector**:
1. **RunEQSQuery Task**: Executes Environment Query
2. Assigns best position to `MoveToLocation` Blackboard key
3. **MoveTo Task**: Navigates to `MoveToLocation`
4. **Attack Task**: Executes ranged attack ability
5. **Wait Task**: Cooldown before next decision

**Behavior Outcome**: Rangers intelligently reposition around obstacles to maintain line of sight, pursuing player through complex environments.

**Design Benefits**:
- **Dynamic Positioning**: No hardcoded attack spots
- **Obstacle Awareness**: Automatically adapts to level geometry
- **Designer Control**: Artists/designers tune EQS parameters without code
- **Reusable**: Same EQ applies to all ranged enemy types
- **Performance**: EQS queries run asynchronously, don't block gameplay thread

**Gameplay Impact**: Creates challenging AI that uses terrain intelligently, forcing players to manage positioning and cover.

---

## Enemy Melee Attacks

### Melee Ability Assignment

**Class-Specific Abilities**: Extend CharacterClassDefaultInfo struct with `StartupAbilities` array.

**AuraMeleeAttack Ability**:
- Inherits from `AuraDamageGameplayAbility`
- Assigned to Warrior class in `CharacterClassInfo` Data Asset

**Ability Granting** (in AuraAbilitySystemLibrary):
- Modify `GiveStartupAbilities()` to loop through class-specific abilities
- Call `GiveAbility()` on ASC for each startup ability

**BT Activation**:
- Assign Gameplay Tag to melee attack ability
- `BTT_Attack` task calls `ActivateAbilityByTag()` when condition met

**Design Benefit**: Class-specific abilities defined in data, enabling different enemy types with zero code changes.

### Motion Warping for Melee

**Component Setup**:
- Add `MotionWarpingComponent` to Enemy class
- Add Motion Warping AnimNotify State to spear attack animation
- Configure to rotate toward `FacingTarget`

**Blueprint Implementation**:
- `UpdateFacingTarget()` - Blueprint implementable event in Enemy
- Calls `AddMotionWarping()` with target data
- Called from `GA_MeleeAttack` during ability execution

**Target Acquisition**:
- `FacingTarget` retrieved via `IEnemyInterface` accessors (Blueprint Native Event)
- Setter implemented in `BTT_Attack` task
- Value sourced from `TargetToFollow` Blackboard key

**Design Pattern**: Interface-based communication decouples ability code from enemy implementation details.

### Animation-Driven Damage Application

**Event Notification**: Same pattern as FireBolt projectile
- `AnimNotify_MontageEvent` placed in spear attack animation
- Sends Gameplay Event with tag `Attack.Melee`

**GA_MeleeAttack Execution**:
1. Wait for Gameplay Event (`Attack.Melee` tag)
2. On event received: Generate damage detection sphere at spear tip
3. Find overlapping actors within radius
4. Apply damage to valid targets

### Sphere Overlap Detection

**AuraAbilitySystemLibrary Function**:
- `GetLivePlayersWithinRadius()` - Blueprint callable, static
- Parameters: World, origin, radius
- Returns: Array of overlapping live actors

**Filtering Logic**:
1. Perform sphere overlap query
2. For each overlapping actor:
   - Check `ICombatInterface::IsDead()` - exclude dead actors
   - Check actor validity
3. Return filtered list

**Interface Additions**:
- `IsDead()` - Death state query
- `GetAvatarActor()` - Retrieve physical actor (handles Owner vs. Avatar distinction)

### Damage Application Architecture

**CauseDamage() Function** (in AuraDamageGameplayAbility):
- **Input**: Target actor
- **Process**:
  1. Create `GameplayEffectSpecHandle`
  2. Loop through ability's `DamageTypes` map
  3. Sum magnitudes across all damage types
  4. Apply accumulated damage to target's ASC via spec

**GA_MeleeAttack Blueprint**:
- Call `GetLivePlayersWithinRadius()` at damage moment
- For each overlapping actor: Call `CauseDamage(Actor)`

**Design Benefit**: Single `CauseDamage()` function handles all damage-dealing abilities, reducing code duplication.

### Tagged Montage System

**Problem**: Different enemies attack from different body parts (weapon hand, left/right claws, head, etc.).

**Challenge**: Need dynamic socket location based on attack variation.

**Solution**: Link montages with Gameplay Tags to identify attack type and corresponding socket.

**Montage Tags** (parent: `Montage.*`):
- `Montage.Attack.Left`
- `Montage.Attack.Right`  
- `Montage.Attack.Weapon`

**FTaggedMontage Struct** (defined in ICombatInterface):
- `UAnimMontage* Montage`
- `FGameplayTag MontageTag`
- **Purpose**: Couples animation with semantic identifier

### Tagged Montage Implementation

**AuraCharacterBase**:
- `TArray<FTaggedMontage> AttackMontages` - Stores all attack variations
- Populated in Blueprint per-enemy type

**ICombatInterface::GetTaggedMontages()**: Returns montage array for character.

**GA_MeleeAttack Usage**:
1. Call `GetAttackMontages()` on enemy
2. Select random montage from array
3. Extract montage and tag from struct
4. Play montage with `PlayMontageAndWait`
5. Pass tag to `WaitGameplayEvent` for timing synchronization

**Animation Notify**: Spear attack animation sends event with montage-specific tag (e.g., `Montage.Attack.Weapon`).

**Design Benefits**:
- **Variety**: Single ability supports multiple attack animations per enemy
- **Extensible**: Add new attacks by adding structs in Blueprint
- **Data-Driven**: Designers configure montages without code

### Dynamic Socket Resolution

**GetCombatSocketLocation() Evolution**: Now accepts Gameplay Tag parameter.

**Implementation** (AuraCharacterBase.cpp):
- Receive montage tag
- Switch/case on tag value
- Return corresponding socket name based on tag

**Example - Ghoul**:
- Two sockets: `LeftHandSocket`, `RightHandSocket` (configured in skeletal mesh)
- `Montage.Attack.Left` → returns `LeftHandSocket`
- `Montage.Attack.Right` → returns `RightHandSocket`

**Goblin Spearman**:
- Single socket: `WeaponTipSocket`
- `Montage.Attack.Weapon` → returns `WeaponTipSocket`

**Design Elegance**: Same ability code handles weaponless Ghouls and armed Goblins through tag-based socket resolution.

### Ghoul Enemy Integration

**Setup Checklist**:
- Create skeletal mesh sockets (left/right wrist)
- Configure Idle/Walk blend space
- Assign Animation Blueprint
- Tune movement speed
- Add Motion Warping component
- Create two Attack Montages (left/right arm)
- Add `AnimNotify_MontageEvent` to each montage with appropriate tag
- Populate `AttackMontages` array in Blueprint

**Friendly Fire Prevention**:
- Modify `GetLivePlayersWithinRadius()` to exclude same-faction actors
- Implementation in `AuraAbilitySystemLibrary` (C++)
- Called in Blueprint before `CauseDamage()`

**Design Outcome**: Multiple enemy types with varied attack patterns sharing the same ability system through tag-based configuration.

---

## Enemy Ranged Attacks

### Ranged Attack Ability Setup

**GA_RangedAttack Configuration**:
- Inherits from `AuraProjectileSpell`
- **Projectile Class**: `BP_SlingshotRock`
- **Damage Effect**: `GE_Damage`
- **Damage Type Map**: Physical damage tag → Ranged damage curve (from `CT_Damage`)

**Ability Assignment**:
- Add `Attack` tag to ability (enables BT activation via `BTT_Attack`)
- Assign to Ranger class `StartupAbilities` in `DA_CharacterClassInfo`

**Animation Setup**:
- Create Attack Montage with `MotionWarping` and `AnimNotify_MontageEvent`
- Populate `TaggedMontages` array in `BP_GoblinSlingshot` Blueprint

**Design Parallel**: Ranged setup mirrors melee pattern - same ability architecture supports different attack modalities.

### Montage Execution Pattern

**GA_RangedAttack Blueprint Flow**:
1. Call `UpdateFacingTarget()` - Sets motion warping target
2. Call `GetRandomAttackMontage()` - Retrieves montage from array
   - **Blueprint Pure Function**: Reduces graph complexity
   - **Applied Retroactively**: Also integrated into `GA_MeleeAttack` for consistency
3. Play montage with `PlayMontageAndWait`
4. Wait for Gameplay Event (tag sent by `AnimNotify_MontageEvent`)
5. On event received: Call `SpawnProjectile()` at combat socket location

**Design Benefit**: Unified execution flow across melee and ranged attacks - only projectile spawning differs.

### Slingshot Pouch Animation System

**Challenge**: Animate slingshot pouch dynamically as it's pulled back and released.

**Animation Blueprint Approach**:
1. Get `OwnerMesh` reference
2. Retrieve `RightHandSocket` transform from mesh
3. Use **Transform (Modify) Bone** node to position pouch relative to hand socket
4. Bind pouch position to hand socket transform

**Animation State Control**:
- **AnimNotify_GrabRock**: Triggers when character grabs ammunition
- **AnimNotify_ReleasePouch**: Triggers when projectile is launched

**HoldingPouch Boolean**:
- Set `false` by `GrabRock` notify
- Set `true` by `ReleasePouch` notify
- Controls attack animation blend (pulling back vs. idle)

**Animation Logic**:
- `HoldingPouch == true` → Play pouch pullback animation
- `HoldingPouch == false` → Return pouch to rest position

**Design Benefit**: 
- **Dynamic**: Pouch follows hand socket automatically through all animations
- **Reusable**: Same ABP pattern applies to any multi-stage ranged weapon (bows, crossbows)
- **Designer-Friendly**: Animation timing controlled via notifies in animation editor, not code

---

## Enemy Spell Attacks

### Shaman Enemy Setup

**Elementalist Class Representative**: New Shaman enemy type demonstrating caster archetype.

**Character Configuration**:
- Create class, Idle/Walk blend space, Animation Blueprint
- Standard animation setup parallels existing enemies

**Attack Animation Setup**:
- Create Attack Montage with Motion Warping component
- Add `AnimNotify_MontageEvent` to send montage tag
- Populate `TaggedMontages` array in `BP_Shaman` Blueprint
- Add `TipSocket` to staff (projectile spawn location, same pattern as player Aura)

### Spell Ability Implementation

**GA_ShamanFireBolt**:
- Inherits from `AuraProjectileSpell` (reuses fire projectile logic)
- Customizes only relevant parameters per Shaman design
- Inherits all projectile spawning, motion warping, ability task infrastructure

**Audio Integration**: Add sound effects to spell execution for environmental feedback.

**Design Simplicity**: Spell abilities require zero new code - inherited architecture handles all complexity, only parameters and audio vary per enemy type.

### Behavior Tree Refinement

**Dead Blackboard Key**: Prevents bugs where deceased enemies continue executing BT logic.
- Check Dead key before ability activation in BT tasks
- Update key in enemy death event

**Dissolve Effect on Shaman**: Add visual flourish matching player Aura's dissolve behavior on death - maintains consistent VFX language across all characters.

---

- Enemy Finishing Touches
  - In this chapter we will add small improvements to enemies: to begin with, a "swoosh" sound effect for the spear goblin (added as a notify track in its attack animation) and footsteps (added as a notify track in its run animation). We also used a multi template to randomize and pitch randomly the spear sound everytime.
  - To add both hit sound/blood effects to the spear goblin, we add a UNiagaraSystem type variable to Character base (each derived will have their own effect), and the sound is added to the TaggedMontage struct. We set them in the editor, and then we trigger them in GA_MeleeAttack in case of hit.
  - The problem now is that sound/blood effects are replicated only to the owning client so other clients can't hear/see anything. To replicate these effects, we must use Gameplay cues. So we create a BP class from gameplay cue (GC_MeleeImpact), and in the event graph e have several parameters to give to the various sound/blood spawn (that will now be triggered in the gameplay cue BP and not in the GA_MeleeAttack anymore). The trigger of the cue happens in MeleeAttack thanks to ExecuteGameplayCueWithParams, from where we can create the params and give them in input, and they will be passed through to the GC_MeleeImpact.
  - Now in GC_MeleeImpact we want to avoid to hardcode the sound, instead we want to identify it based on the tag of the montage we pass through. Problem is if we have more tagged montages with that tag, it will just pick the first one. So we want to retrieve the correct tagged montage thanks to the separation of montage and socket tags (as for now, these "tagged montages" were actually mainly used to understand which socket to use for each attack/enemy).
  - So old montage tags now are called socket tags, while we create 4 generic montage tags. The tagged montage struct now contains both a montage tag and a socket tag. Now in the anim events we send the (new) montage tags, while (old montage) socket tags are actually used to retrieve the locations of the sockets for spawning whatever sound effect etc. In cpp we create a function that takes a tag and returns the corresponding tagged montage struct. We call this function in the gameplay cue in order to get the tagged montage we are interested in, and from there we take the corresponding unique sound associated.
  - For other small additions, same as in the first note of this section, we add hurt sounds in BP (in the Anim Montage of the HitReact) and death sounds in C++ (in multiDeath function implementation) to goblin spears and goblin slingshots (also footsteps for them). Then we make rock impact sound and impact effects for goblin slingshots, same for shaman (also hit react for him as it was never created), and same for the sounds of hurt/death/swipe of the Ghoul (for both left/right attack). Finally, we add particle niagara trail effects to the ghouls claw swipe (as a notify state in the left/right attack anim montages).
  - Now we want to create the last kind of enemy, the Demon: we do all the usual stuff to create/assign BP/Anim/TailSocket etc., so MotionWarping, AN_MontageEvents and Swipe sounds.