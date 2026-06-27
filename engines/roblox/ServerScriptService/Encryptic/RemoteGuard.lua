local Players = game:GetService("Players")

local RemoteGuard = {}
local maxPerSecond = 40
local counts: { [Player]: { number } } = {}
local onViolation: ((Player, string) -> ())? = nil

function RemoteGuard.start(opts: number | { maxRemotePerSecond: number?, onViolation: ((Player, string) -> ())? })
	if typeof(opts) == "number" then
		maxPerSecond = opts
	else
		maxPerSecond = opts.maxRemotePerSecond or maxPerSecond
		onViolation = opts.onViolation
	end

	Players.PlayerRemoving:Connect(function(player)
		counts[player] = nil
	end)
end

function RemoteGuard.track(player: Player): boolean
	local now = os.clock()
	local history = counts[player]
	if not history then
		history = {}
		counts[player] = history
	end

	table.insert(history, now)
	while #history > 0 and now - history[1] > 1 do
		table.remove(history, 1)
	end

	if #history > maxPerSecond then
		if onViolation then
			onViolation(player, string.format("Remote spam count=%d max=%d", #history, maxPerSecond))
		end
		return false
	end

	return true
end

-- Wrap a RemoteEvent OnServerEvent with automatic rate limiting
function RemoteGuard.wrapRemoteEvent(remote: RemoteEvent, handler: (Player, ...any) -> ())
	remote.OnServerEvent:Connect(function(player, ...)
		if not RemoteGuard.track(player) then
			return
		end
		handler(player, ...)
	end)
end

return RemoteGuard
