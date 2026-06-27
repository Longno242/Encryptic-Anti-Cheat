local ToolGuard = {}
local maxEquipPerSecond = 8
local history: { [Player]: { number } } = {}
local onViolation: ((Player, string) -> ())? = nil

function ToolGuard.start(opts: { maxEquipPerSecond: number?, onViolation: ((Player, string) -> ())? })
	maxEquipPerSecond = opts.maxEquipPerSecond or maxEquipPerSecond
	onViolation = opts.onViolation
end

function ToolGuard.trackEquip(player: Player): boolean
	local now = os.clock()
	local list = history[player]
	if not list then
		list = {}
		history[player] = list
	end

	table.insert(list, now)
	while #list > 0 and now - list[1] > 1 do
		table.remove(list, 1)
	end

	if #list > maxEquipPerSecond then
		if onViolation then
			onViolation(player, "Tool equip spam")
		end
		return false
	end

	return true
end

return ToolGuard
