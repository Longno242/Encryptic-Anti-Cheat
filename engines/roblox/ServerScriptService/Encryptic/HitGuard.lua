local HitGuard = {}
local maxHitDistance = 20
local onViolation: ((Player, string) -> ())? = nil

function HitGuard.start(opts: { maxHitDistance: number?, onViolation: ((Player, string) -> ())? })
	maxHitDistance = opts.maxHitDistance or maxHitDistance
	onViolation = opts.onViolation
end

-- Call from your combat system before applying damage
function HitGuard.validateHit(attacker: Player, targetChar: Model, hitPosition: Vector3?): boolean
	local attackerChar = attacker.Character
	local attackerRoot = attackerChar and attackerChar:FindFirstChild("HumanoidRootPart") :: BasePart?
	local targetRoot = targetChar and targetChar:FindFirstChild("HumanoidRootPart") :: BasePart?
	if not attackerRoot or not targetRoot then
		return false
	end

	local from = hitPosition or targetRoot.Position
	local dist = (attackerRoot.Position - from).Magnitude
	if dist > maxHitDistance then
		if onViolation then
			onViolation(attacker, string.format("Hit distance exploit dist=%.1f", dist))
		end
		return false
	end

	return true
end

return HitGuard
