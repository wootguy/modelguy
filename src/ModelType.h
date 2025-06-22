#pragma once
#include <vector>

enum model_types {
	PMODEL_UNKNOWN,
	PMODEL_EXTERNAL, // external texture or animation model
	PMODEL_HALF_LIFE,
	PMODEL_TEAM_FORTRESS,
	PMODEL_COUNTER_STRIKE,
	PMODEL_SVEN_COOP_3,
	PMODEL_SVEN_COOP_4,
	PMODEL_SVEN_COOP_5,
	PMODEL_DAY_OF_DEFEAT,
	PMODEL_RICOCHET,
	PMODEL_VAMPIRE_SLAYER,
	PMODEL_THE_SPECIALISTS,
	PMODEL_ACTION_HALF_LIFE,
	PMODEL_EARTHS_SPECIAL_FORCES,
};

struct AnimDesc {
	int activity;
	const char* name;
};

struct ModelType {
	const char* modname;
	int modcode;
	int max_texture_size;
	std::vector<AnimDesc> anims;
	int num_match_act;
	int num_match_name;
	float match_percent;
};

extern std::vector<ModelType> g_modelTypes;