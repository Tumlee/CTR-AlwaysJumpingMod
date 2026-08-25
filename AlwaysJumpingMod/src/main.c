#include <common.h>

//The address of VehPhysGeneral_JumpAndFriction()
#define ADDR_VehPhysGeneral_JumpAndFriction ((void(*)(struct Thread*, struct Driver*))0x80060630)
#define ADDR_BOT_DRIVE ((void(*)(struct Thread*)) 0x80013c18)

//Wrapper function that gets called in place of the real VehPhysGeneral_JumpAndFriction()
void DOC_JumpAndFriction_Wrapper(struct Thread* thread, struct Driver* driver)
{
	driver->jump_TenBuffer = 10;					//Force a jump to be buffered.
	VehPhysGeneral_JumpAndFriction(thread, driver);	//Run the real function (which checks )
	driver->jump_TenBuffer = 10;					//Probably unnecessary but this is harmless.
}

void DOC_Bot_ThTick_Drive_Wrapper(struct Thread* botThread)
{
	struct Driver* driver = (struct Driver*)botThread->object;

	//Call the real BOT behavior function.
	BOTS_ThTick_Drive(botThread);

	//The bots can get stuck or slow down severely near corners unless we allow them to accelerate
	//a little bit before they start hopping. 
	int minimumHopSpeed = (FP8_ONE * 18);
	int jumpForce = driver->const_JumpForce;

	if(sdata->gGT->gameMode1 < 0)	//If this is a boss race.
	{
		//Allow bosses to gain a little more speed than normal AI.
		minimumHopSpeed = (FP8_ONE * 22);
	}

	bool isGrounded = (driver->actionsFlagSet & 1) != 0;

	//Force the bot to jump. This is a quick and dirty implementation.
	//Because it doesn't check the slope of the QuadBlock they're on, they will
	//end up jumping higher than expected on downhills and they may fail to leave
	//the ground on uphills. 
	if (isGrounded && driver->speedApprox > minimumHopSpeed)
		driver->botData.unk5bc.ai_speedY = jumpForce;
}

//Find VehPhysGeneral_JumpAndFriction() in the function pointers array and
//replace it with our wrapper function. Our code may not end up running on frames where
//the kart changes state, but because the buffer is 10 frames long, it should make
//the kart always jumping anyway.
void DOC_PatchFunctionPtrs(struct Driver* driver)
{
	for (int i = 0; i < 13; i++)
    {
        if (driver->funcPtrs[i] == ADDR_VehPhysGeneral_JumpAndFriction)
		{
			driver->funcPtrs[i] = (void*)DOC_JumpAndFriction_Wrapper;
			break;
		}
    }
}

void AlwaysJumpingMain()
{
	//FIXME: This appears to work, but the code may fire outside of races, not sure.
	//We really should check the actual game state rather than just checking the number of players.
	if(sdata->gGT->numPlyrCurrGame == 1)
	{
		//Loop through everyone in the PLAYER bucket
		struct Thread* initialThread = sdata->gGT->threadBuckets[PLAYER].thread;

		for(struct Thread* thread = initialThread; thread != NULL; thread = thread->siblingThread)
		{
			struct Driver* driver = (struct Driver*)thread->object;

			//Pretty sure that when a player finishes a race, they have BOT_ functions assigned to them
			//but they're still in the PLAYER bucket. Could be wrong about this. This is either necessary
			//or it does nothing and it's harmless.
			if(thread->funcThTick == ADDR_BOT_DRIVE)
				thread->funcThTick = (void*)DOC_Bot_ThTick_Drive_Wrapper;

			DOC_PatchFunctionPtrs(driver);
		}

		//Loop through everyone in the ROBOT bucket.
		initialThread = sdata->gGT->threadBuckets[ROBOT].thread;

		for(struct Thread* thread = initialThread; thread != NULL; thread = thread->siblingThread)
		{
			struct Driver* driver = (struct Driver*)thread->object;

			//For all other AI drivers
			if(thread->funcThTick == ADDR_BOT_DRIVE)
				thread->funcThTick = (void*)DOC_Bot_ThTick_Drive_Wrapper;
		}
	}
}

