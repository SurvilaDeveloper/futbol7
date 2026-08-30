# Futbol7

[Español](README_ES.md)

**Futbol7** is a work-in-progress soccer gameplay project developed in **Unreal Engine 4.27 with C++**.

> **Technical note:** the Unreal project and C++ module are currently still named `ThirdPersonCpp`, because the project originally started from Unreal's default Third Person template. The public/project name is now **Futbol7**. The internal Unreal project/module name may be migrated later.

The main goal is to build a soccer game where human-controlled players and AI-controlled teammates/opponents share the same match environment, rules, ball interactions, tactical behavior, animations and restart systems.

This repository is under active development. The current focus is not visual polish or a finished game loop, but building a solid gameplay foundation that can later support a complete soccer game.

---

## Current focus

Development is currently centered on:

- AI decision-making and tactical positioning
- Human and AI interaction with the ball
- Match rules and restart states
- Goalkeeper behavior
- Passing, shooting, dribbling and interception logic
- Aerial ball interactions
- Tackles, fouls and penalties
- Animation synchronization with gameplay
- Refactoring match situations into cleaner systems
- Making gameplay logic depend on field dimensions instead of hard-coded positions

A large part of the work consists of repeatedly testing match situations, finding edge cases and refining the interaction between gameplay rules, navigation, animation and AI.

---

## Implemented gameplay systems

### Match flow and rules

The project already contains working or partially developed systems for:

- Kickoff
- Throw-ins
- Goal kicks
- Corner kicks
- Free kicks
- Offside restarts
- Penalties
- Goals and ball-out detection
- Possession tracking
- Last-touch tracking
- Double-touch restrictions on restarts
- Preparation and execution phases for set pieces

Restart systems are progressively being organized into clearer phases:

1. Configuration
2. Preparation
3. Execution

This makes it easier to control player positioning, ball interaction and AI behavior before the match returns to normal play.

---

## Human-controlled player

The human player currently supports systems such as:

- Manual movement
- Ball chasing
- Passing
- Shooting
- Dribbling / self-passes
- Tackling
- Aerial actions
- Pass requests to AI teammates
- Receiving AI passes
- Taking selected match restarts

The human player can dynamically become the taker of several restart situations when close enough to the ball. If the player moves away before committing to the restart, an AI teammate can take over automatically.

Current human restart support includes:

- Free kicks
- Kickoffs
- Goal kicks
- Corner kicks
- Throw-ins

Throw-ins use a dedicated hand-based sequence rather than the normal kicking system.

---

## Soccer AI

AI-controlled players are being developed around tactical roles and contextual behavior rather than simple ball chasing.

Current AI work includes:

- Team attacking and defending phases
- Pressure, support and cover responsibilities
- Positional team shape
- Run-into-space behavior
- Defensive marking
- Rest-defense positioning
- Dangerous-receiver marking
- Predictive ball interception
- Passing decisions
- Self-pass behavior
- Human-player pass requests
- Restart taker and receiver logic
- Navigation recovery when a player ends up outside the playable field

AI players can also choose the human-controlled player as a receiver during several restart situations when the pass is considered appropriate.

---

## Goalkeeper systems

Goalkeepers have their own gameplay logic, including:

- Ball capture with the hands
- Protected possession while holding the ball
- Distribution by hand
- Goal positioning
- Shot-response logic
- Penalty positioning
- Return-to-goal behavior
- Dedicated movement and animation handling

Goalkeeper behavior is still being refined, especially around positioning, recovery and decision-making after saves or aggressive movements away from goal.

---

## Passing and interception

Ball movement is not based only on chasing the ball's current position.

The project includes predictive interception logic used by both AI and human-controlled ball-chasing systems. Players can estimate where the ball is moving and attempt to intercept its future path instead of following it directly.

Passing systems distinguish between different intentions, including:

- Passes to teammates
- Passes requested by the human player
- Aerial passes
- Self-passes
- Restart passes

AI teammates are expected to trust an intentional pass and avoid immediately treating the ball as free possession.

---

## Tackles, fouls and fall reactions

The project includes sliding tackle mechanics with animation-aware foul detection.

Current work includes:

- Left-leg and right-leg tackle sequences
- Contact detection
- Foul recognition
- Penalty detection inside the penalty area
- Fall reactions for both AI and human players
- Recovery after falling
- Handling players that finish an animation outside the navigation area

The system is designed so animation timing can affect gameplay rather than being purely cosmetic.

---

## Aerial ball interactions

Several aerial interactions are being developed, including:

- Standing headers
- Chest control
- Jumping headers
- Aerial challenges
- Directed aerial rebounds

Animation data exported from Blender is also used to help synchronize contact between the player and the ball.

---

## Animation system

The project uses animation assets imported into Unreal Engine and synchronized with C++ gameplay logic.

Examples include:

- Standing passes
- Running passes
- Side passes
- Long strikes
- Tackles
- Fall reactions
- Throw-ins
- Goalkeeper actions
- Headers and chest control

AI players and the human-controlled player maintain separate gameplay logic where useful, even when they use similar animation assets.

For AI kicking, the ball can remain stationary until the actual impact moment of the montage instead of being launched immediately when the AI decides to kick.

---

## Field geometry

An important project goal is to avoid hard-coded soccer-field positions.

Field-related calculations are intended to depend on the project's field-dimension system so gameplay logic can adapt if the pitch size or stadium configuration changes in the future.

This affects systems such as:

- Restart locations
- Penalty positioning
- Defensive and offensive positioning
- Off-field recovery
- Goal references
- Tactical spacing

---

## Debugging and testing

The project contains dedicated debugging tools used to visualize and test gameplay systems.

Debug work includes:

- Predicted trajectories
- Interception points
- Player targets
- Tactical positions
- Goalkeeper behavior
- Aerial contact points
- Match-state information

Debug visualization is progressively being centralized so gameplay classes do not each maintain unrelated debug systems.

---

## Technology

- **Unreal Engine 4.27**
- **C++**
- **Visual Studio 2022**
- **Blender**
- **Mixamo-based character assets and animations**
- Unreal Animation Blueprints and Montages
- Unreal Navigation System / NavMesh

---

## Project status

This is an **experimental and actively evolving project**.

Many systems already work in gameplay, but they are continuously tested and adjusted as new interactions expose edge cases. Refactoring is an important part of the development process because soccer situations often interact with several systems at once: AI, navigation, physics, rules, animation and player input.

The repository should therefore be considered a development snapshot rather than a finished product.

---

## Development direction

Upcoming work will continue to focus on:

- Improving tactical AI
- Refining goalkeeper decisions
- Expanding aerial and defensive interactions
- Improving restart behavior
- Better animation/gameplay synchronization
- Reducing edge cases caused by navigation and physical interactions
- Continuing the match-state refactor
- Building toward a more complete playable soccer match

---

## Notes

Futbol7 is primarily being developed as a learning and experimentation environment for **gameplay programming, artificial intelligence, animation integration and soccer simulation systems**.

The codebase changes frequently as systems are tested and redesigned.
