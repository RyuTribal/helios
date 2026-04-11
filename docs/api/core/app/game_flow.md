# GameFlow

The `GameFlow<StateEnum>` class manages a stack of active game states. It allows for complex transitions and state-specific system registration.

## Methods

### Stack Operations

| Method | Description |
| :--- | :--- |
| `go_to<States...>()` | Clears the stack and pushes one or more new states. |
| `push<S>()` | Pushes a state on top of the stack. |
| `pop()` | Removes the topmost state from the stack. |
| `switch_to<S>()` | Replaces the topmost state with a new state. |

### Queries

| Method | Description |
| :--- | :--- |
| `current()` | Returns the enum value of the topmost state. |
| `is_in(StateEnum state)` | Checks whether the specified state is anywhere on the stack. |
| `is_empty()` | Checks whether the state stack is empty. |
| `depth()` | Returns the number of states on the stack. |

### Registration

| Method | Description |
| :--- | :--- |
| `register_state<S>(StateEnum id)` | Registers a concrete state class associated with an enum ID. |
| `transition(StateEnum from, StateEnum to)` | Defines a transition chain between two states. |

## Usage Example

```cpp
enum class GameState { MainMenu, Playing, Pause };

void menu_system(ResMut<GameFlow<GameState>> flow) {
    if (start_pressed) {
        flow->go_to<Playing>();
    }
}

void playing_system(ResMut<GameFlow<GameState>> flow) {
    if (escape_pressed) {
        flow->push<Pause>();
    }
}
```

## Related Pages
- [App](app.md)
- [Scheduler](../ecs/scheduler.md)
