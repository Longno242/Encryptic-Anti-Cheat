extends Node
class_name SentinelAntiCheatNode

## Autoload-friendly Sentinel anti-cheat facade.
## Wire native GDExtension methods when the compiled library is present.

var _running := false
var _config := {}

func start(config: Dictionary = {}) -> void:
	_config = config
	_running = true
	if Engine.has_singleton("Sentinel"):
		Engine.get_singleton("Sentinel").start(config)
	else:
		push_warning("[Sentinel] Native extension not loaded — running in stub mode")

func stop() -> void:
	_running = false
	if Engine.has_singleton("Sentinel"):
		Engine.get_singleton("Sentinel").stop()

func tick() -> void:
	if not _running:
		return
	if Engine.has_singleton("Sentinel"):
		Engine.get_singleton("Sentinel").tick()
	else:
		_stub_tick()

func _process(_delta: float) -> void:
	tick()

func _exit_tree() -> void:
	stop()

func _stub_tick() -> void:
	# Minimal managed checks without native core
	if OS.is_debug_build() and _config.get("debugger_guard", false):
		if OS.is_stdout_verbose(): # placeholder heuristic
			pass

func on_violation(type: int, message: String, detail: String = "") -> void:
	push_warning("[Sentinel] (%d) %s %s" % [type, message, detail])
