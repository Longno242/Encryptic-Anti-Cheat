extends Node
class_name EncrypticAntiCheatNode

var _running := false
var _config := {}

func start(config: Dictionary = {}) -> void:
	_config = config
	_running = true
	if Engine.has_singleton("Encryptic"):
		Engine.get_singleton("Encryptic").start(config)
	else:
		push_warning("[Encryptic] Native extension not loaded — stub mode")

func stop() -> void:
	_running = false
	if Engine.has_singleton("Encryptic"):
		Engine.get_singleton("Encryptic").stop()

func tick() -> void:
	if not _running:
		return
	if Engine.has_singleton("Encryptic"):
		Engine.get_singleton("Encryptic").tick()

func _process(_delta: float) -> void:
	tick()

func _exit_tree() -> void:
	stop()

func on_violation(type: int, message: String, detail: String = "") -> void:
	push_warning("[Encryptic] (%d) %s %s" % [type, message, detail])
