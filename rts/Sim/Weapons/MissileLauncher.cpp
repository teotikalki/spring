/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "MissileLauncher.h"

#include "WeaponDef.h"
#include "Game/TraceRay.h"
#include "Map/Ground.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectileFactory.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "System/myMath.h"

CR_BIND_DERIVED(CMissileLauncher, CWeapon, (NULL, NULL))
CR_REG_METADATA(CMissileLauncher, )

CMissileLauncher::CMissileLauncher(CUnit* owner, const WeaponDef* def): CWeapon(owner, def)
{
}


void CMissileLauncher::UpdateWantedDir()
{
	CWeapon::UpdateWantedDir();

	if (weaponDef->trajectoryHeight > 0.0f) {
		wantedDir.y += weaponDef->trajectoryHeight;
		wantedDir.Normalize();
	}
}

void CMissileLauncher::FireImpl(const bool scriptCall)
{
	float3 dir = currentTargetPos - weaponMuzzlePos;
	const float dist = dir.LengthNormalize();

	if (onlyForward) {
		dir = owner->frontdir;
	} else if (weaponDef->fixedLauncher) {
		dir = weaponDir;
	} else if (weaponDef->trajectoryHeight > 0.0f) {
		dir.y += weaponDef->trajectoryHeight;
		dir.Normalize();
	}

	dir += (gsRNG.NextVector() * SprayAngleExperience() + SalvoErrorExperience());
	dir.Normalize();

	float3 startSpeed = dir * weaponDef->startvelocity;

	// NOTE: why only for SAMT units?
	if (onlyForward && owner->unitDef->IsStrafingAirUnit())
		startSpeed += owner->speed;

	ProjectileParams params = GetProjectileParams();
	params.pos = weaponMuzzlePos;
	params.end = currentTargetPos;
	params.speed = startSpeed;
	params.ttl = weaponDef->flighttime == 0? math::ceil(std::max(dist, range) / projectileSpeed + 25 * weaponDef->selfExplode): weaponDef->flighttime;

	WeaponProjectileFactory::LoadProjectile(params);
}

bool CMissileLauncher::HaveFreeLineOfFire(const float3 srcPos, const float3 tgtPos, const SWeaponTarget& trg) const
{
	// do a different test depending on if the missile has high
	// trajectory (parabolic vs. linear ground intersection)
	if (weaponDef->trajectoryHeight <= 0.0f)
		return (CWeapon::HaveFreeLineOfFire(srcPos, tgtPos, trg));

	float3 dir(tgtPos - srcPos);
	float3 flatDir(dir.x, 0, dir.z);
	dir.SafeNormalize();
	const float flatLength = flatDir.LengthNormalize();

	if (flatLength == 0.0f)
		return true;

	const float linear = dir.y + weaponDef->trajectoryHeight;
	const float quadratic = -weaponDef->trajectoryHeight / flatLength;
	const float groundDist = ((avoidFlags & Collision::NOGROUND) == 0)?
		CGround::TrajectoryGroundCol(srcPos, flatDir, flatLength, linear, quadratic):
		-1.0f;

	if (groundDist > 0.0f)
		return false;

	if (TraceRay::TestTrajectoryCone(srcPos, flatDir, flatLength,
		linear, quadratic, 0.0f, owner->allyteam, avoidFlags, owner)) {
		return false;
	}

	return true;
}
