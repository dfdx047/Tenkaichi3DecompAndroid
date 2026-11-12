# cod_1C3400.data: the shape of this table, without its values (port/tools/make_skeleton.py)
.section .data
.p2align 4
.globl gOverlayTbl
gOverlayTbl:
    .long D_002EB398
    .long D_002EB380
.globl D_002C3408
D_002C3408:
    .space 8
.globl gTexBltPacket
gTexBltPacket:
    .space 48
.globl D_002C3440
D_002C3440:
    .space 16
.globl gMathfSinCoef
gMathfSinCoef:
    .space 16
.globl gSndBankVolume
gSndBankVolume:
    .space 32
.globl gBattleTimeLimitTbl
gBattleTimeLimitTbl:
    .space 16
.globl gBtlSceneRootClass
gBtlSceneRootClass:
    .long BtlSceneRoot_Update
    .long BtlSceneRoot_Init
    .long BtlSceneRoot_Term
    .space 12
.globl D_002C34A8
D_002C34A8:
    .space 8
.globl gEftBubbleZeroVel
gEftBubbleZeroVel:
    .space 16
.globl gEftBubbleClass
gEftBubbleClass:
    .long EftBubble_Update
    .long EftBubble_Init
    .long EftBubble_Term
    .long EftBubble_PostUpdate
    .space 4
    .long EftBubble_Draw
.globl gEftStageScrollClass
gEftStageScrollClass:
    .long EftStageScroll_Update
    .long EftStageScroll_Init
    .long EftStageScroll_Term
    .space 4
    .long EftStageScroll_Reset
    .long EftStageScroll_DrawStub
.globl gEftGeyserMgrClass
gEftGeyserMgrClass:
    .long EftGeyser_MgrUpdate
    .long EftGeyser_MgrInit
    .long EftGeyser_MgrTerm
    .space 12
.globl gEftGeyserClass
gEftGeyserClass:
    .long EftGeyser_Update
    .long EftGeyser_Init
    .long EftGeyser_Term
    .space 4
    .long EftGeyser_ResetStub
    .long EftGeyser_Draw
.globl D_002C3520
D_002C3520:
    .long EftWeather_Update
    .long EftWeather_Init
    .long EftWeather_Term
    .space 12
.globl gEftWeatherPtclClass
gEftWeatherPtclClass:
    .long EftWeatherPtcl_Update
    .long EftWeatherPtcl_Init
    .long EftWeatherPtcl_Term
    .long EftWeatherPtcl_PostUpdate
    .long EftWeatherPtcl_Reset
    .long EftWeatherPtcl_Draw
.globl D_002C3550
D_002C3550:
    .long EftStage_Update
    .long EftStage_Init
    .long EftStage_Term
    .space 12
.globl gEftStageKinds
gEftStageKinds:
    .long gEftStageScrollClass
    .space 4
    .long D_002C35B8
    .space 4
    .long D_002C3520
    .space 4
    .long gEftStormClass
    .space 4
    .long gEftSmokeMgrClass
    .space 4
    .long D_002C3600
    .space 4
    .long gEftGeyserMgrClass
    .space 4
    .long D_002C3640
    .space 4
    .long gEftBubbleClass
    .space 4
    .long gEftBoundMgrClass
    .space 4
.globl D_002C35B8
D_002C35B8:
    .long EftSurf_Update
    .long EftSurf_Init
    .long EftSurf_Term
    .long EftSurf_PostUpdate
    .long EftSurf_Reset
    .long EftSurf_Draw
.globl D_002C35D0
D_002C35D0:
    .long EftBurstLayer_Update
    .long EftBurstLayer_Init
    .long EftBurstLayer_Term
    .space 12
.globl gEftBurstClass
gEftBurstClass:
    .long EftBurst_Update
    .long EftBurst_Init
    .long EftBurst_Term
    .long EftBurst_PostUpdate
    .long EftBurst_Reset
    .long EftBurst_Draw
.globl D_002C3600
D_002C3600:
    .long EftSteamMgr_Update
    .long EftSteamMgr_Init
    .long EftSteamMgr_Term
    .space 12
.globl gEftSteamClass
gEftSteamClass:
    .long EftSteam_Update
    .long EftSteam_Init
    .long EftSteam_Term
    .space 4
    .long EftSteam_Reset
    .long EftSteam_Draw
.globl D_002C3630
D_002C3630:
    .space 16
.globl D_002C3640
D_002C3640:
    .long EftWater_Update
    .long EftWater_Init
    .long EftWater_Term
    .long EftWater_PostUpdate
    .long EftWater_Reset
    .long EftWater_Draw
.globl gEftStormClass
gEftStormClass:
    .long EftStorm_Update
    .long EftStorm_Init
    .long EftStorm_Term
    .long EftStorm_PostUpdate
    .long EftStorm_Reset
    .long EftStorm_Draw
.globl gEftSmokeMgrClass
gEftSmokeMgrClass:
    .long EftSmokeMgr_Update
    .long EftSmokeMgr_Init
    .long EftSmokeMgr_Term
    .space 12
.globl gEftSmokeClass
gEftSmokeClass:
    .long EftSmoke_Update
    .long EftSmoke_Init
    .long EftSmoke_Term
    .space 4
    .long EftSmoke_Reset
    .long EftSmoke_Draw
.globl gEftBoundMgrClass
gEftBoundMgrClass:
    .long EftBoundMgr_Update
    .long EftBoundMgr_Init
    .long EftBoundMgr_Term
    .space 12
.globl gEftBoundClass
gEftBoundClass:
    .long EftBound_Update
    .long EftBound_Init
    .long EftBound_Term
    .long EftBound_PostUpdate
    .long EftBound_Reset
    .long EftBound_Draw
.globl D_002C36D0
D_002C36D0:
    .long EftShotMgr_Update
    .long EftShotMgr_Init
    .long EftShotMgr_Term
    .space 12
.globl gEftShotCharClass
gEftShotCharClass:
    .long EftShotChar_Update
    .long EftShotChar_Init
    .long EftShotChar_Term
    .space 12
.globl gEftShotClass
gEftShotClass:
    .long gEftShotNullGroupClass
    .long gEftShotNullClass
    .space 4
    .long D_002C38C8
    .long D_002C38E0
    .space 4
    .long gEftVolleyGroupClass
    .long gEftVolleyClass
    .space 4
    .long gEftShotTechMgrClass
    .long gEftShotTechClass
    .space 4
    .long gEftSweepGroupClass
    .long gEftSweepClass
    .space 4
    .long gEftRingShotMgrClass
    .long gEftRingShotClass
    .space 4
    .long D_002C3868
    .long D_002C3880
    .space 4
    .long D_002C3898
    .long D_002C38B0
    .space 4
    .long D_002C3838
    .long gEftFollowClass
    .space 4
    .long gEftObjTechMgrClass
    .long gEftObjTechClass
    .space 4
    .long gEftRushShotMgrClass
    .long gEftRushShotClass
    .space 4
.globl D_002C3784
D_002C3784:
    .space 4
.globl gEftShotNullGroupClass
gEftShotNullGroupClass:
    .long EftShotNullGroup_Update
    .long EftShotNullGroup_Init
    .long EftShotNullGroup_Term
    .space 12
.globl gEftShotNullClass
gEftShotNullClass:
    .long EftShotNull_Update
    .long EftShotNull_Init
    .long EftShotNull_Term
    .long EftShotNull_PostUpdate
    .long EftShotNull_Reset
    .long EftShotNull_Draw
.globl gEftVolleyGroupClass
gEftVolleyGroupClass:
    .long EftVolleyGroup_Update
    .long EftVolleyGroup_Init
    .long EftVolleyGroup_Term
    .space 4
    .long EftVolleyGroup_Reset
    .space 4
.globl gEftVolleyClass
gEftVolleyClass:
    .long EftVolley_Update
    .long EftVolley_Init
    .long EftVolley_Term
    .long EftVolley_PostUpdate
    .long EftVolley_Reset
    .long EftVolley_Draw
.globl gEftEmitNodeSlot
gEftEmitNodeSlot:
    .space 32
.globl gEftSweepGroupClass
gEftSweepGroupClass:
    .long EftSweepGroup_Update
    .long EftSweepGroup_Init
    .long EftSweepGroup_Term
    .space 4
    .long EftSweepGroup_Reset
    .space 4
.globl gEftSweepClass
gEftSweepClass:
    .long EftSweep_Update
    .long EftSweep_Init
    .long EftSweep_Term
    .long EftSweep_PostUpdate
    .long EftSweep_Reset
    .long EftSweep_Draw
.globl D_002C3838
D_002C3838:
    .long EftFollowGroup_Update
    .long EftFollowGroup_Init
    .long EftFollowGroup_Term
    .space 4
    .long EftFollowGroup_Reset
    .space 4
.globl gEftFollowClass
gEftFollowClass:
    .long EftFollow_Update
    .long EftFollow_Init
    .long EftFollow_Term
    .long EftFollow_PostUpdate
    .long EftFollow_Reset
    .long EftFollow_Draw
.globl D_002C3868
D_002C3868:
    .long EftMultiMgr_Update
    .long EftMultiMgr_Init
    .long EftMultiMgr_Term
    .space 4
    .long EftMultiMgr_Reset
    .space 4
.globl D_002C3880
D_002C3880:
    .long EftMulti_Update
    .long EftMulti_Init
    .long EftMulti_Term
    .long EftMulti_PostUpdate
    .long EftMulti_Reset
    .long EftMulti_Draw
.globl D_002C3898
D_002C3898:
    .long EftPropShotMgr_Update
    .long EftPropShotMgr_Init
    .long EftPropShotMgr_Term
    .space 4
    .long EftPropShotMgr_Reset
    .space 4
.globl D_002C38B0
D_002C38B0:
    .long EftPropShot_Update
    .long EftPropShot_Init
    .long EftPropShot_Term
    .long EftPropShot_PostUpdate
    .long EftPropShot_Reset
    .long EftPropShot_Draw
.globl D_002C38C8
D_002C38C8:
    .long EftBlastMgr_Update
    .long EftBlastMgr_Init
    .long EftBlastMgr_Term
    .space 4
    .long EftBlastMgr_Reset
    .space 4
.globl D_002C38E0
D_002C38E0:
    .long EftBlast_Update
    .long EftBlast_Init
    .long EftBlast_Term
    .long EftBlast_PostUpdate
    .long EftBlast_Reset
    .long EftBlast_Draw
.globl gEftShotTechMgrClass
gEftShotTechMgrClass:
    .long EftShotTechMgr_Update
    .long EftShotTechMgr_Init
    .long EftShotTechMgr_Term
    .space 4
    .long EftShotTechMgr_Reset
    .space 4
.globl gEftShotTechClass
gEftShotTechClass:
    .long EftShotTech_Update
    .long EftShotTech_Init
    .long EftShotTech_Term
    .long EftShotTech_PostUpdate
    .long EftShotTech_Reset
    .long EftShotTech_Draw
.globl gEftTechEvtClass
gEftTechEvtClass:
    .long EftTechEvt_Update
    .long EftTechEvt_Init
    .long EftTechEvt_Term
    .space 12
.globl gEftTechEvtTaskClass
gEftTechEvtTaskClass:
    .long EftTechEvtTask_Update
    .long EftTechEvtTask_Init
    .long EftTechEvtTask_Term
    .space 4
    .long EftTechEvtTask_Reset
    .long EftTechEvtTask_Draw
.globl gEftObjTechMgrClass
gEftObjTechMgrClass:
    .long EftObjTechMgr_Update
    .long EftObjTechMgr_Init
    .long EftObjTechMgr_Term
    .space 4
    .long EftObjTechMgr_Reset
    .space 4
.globl gEftObjTechClass
gEftObjTechClass:
    .long EftObjTech_Update
    .long EftObjTech_Init
    .long EftObjTech_Term
    .long EftObjTech_PostUpdate
    .long EftObjTech_Reset
    .long EftObjTech_Draw
.globl gEftRushShotMgrClass
gEftRushShotMgrClass:
    .long EftRushShotMgr_Update
    .long EftRushShotMgr_Init
    .long EftRushShotMgr_Term
    .space 4
    .long EftRushShotMgr_Reset
    .space 4
.globl gEftRushShotClass
gEftRushShotClass:
    .long EftRushShot_Update
    .long EftRushShot_Init
    .long EftRushShot_Term
    .long EftRushShot_PostUpdate
    .long EftRushShot_Reset
    .long EftRushShot_Nop
.globl gEftRingShotMgrClass
gEftRingShotMgrClass:
    .long EftRingShotMgr_Update
    .long EftRingShotMgr_Init
    .long EftRingShotMgr_Term
    .space 4
    .long EftRingShotMgr_Reset
    .space 4
.globl gEftRingShotClass
gEftRingShotClass:
    .long EftRingShot_Update
    .long EftRingShot_Init
    .long EftRingShot_Term
    .long EftRingShot_PostUpdate
    .long EftRingShot_Reset
    .long EftRingShot_Nop
.globl gEftAbsorbMgrClass
gEftAbsorbMgrClass:
    .long EftAbsorbMgr_Update
    .long EftAbsorbMgr_Init
    .long EftAbsorbMgr_Term
    .space 12
.globl gEftAbsorbClass
gEftAbsorbClass:
    .long EftAbsorb_Update
    .long EftAbsorb_Init
    .long EftAbsorb_Term
    .long EftAbsorb_PostUpdate
    .long EftAbsorb_Reset
    .long EftAbsorb_Nop
.globl D_002C3A18
D_002C3A18:
    .long EftSpdLine_Update
    .long EftSpdLine_Init
    .long EftSpdLine_Term
    .long EftSpdLine_Stub
    .space 4
    .long EftSpdLine_Draw
.globl gEftAuraMgrClass
gEftAuraMgrClass:
    .long EftAuraMgr_Update
    .long EftAuraMgr_Init
    .long EftAuraMgr_Term
    .space 4
    .long EftAuraMgr_Stub
    .space 4
.globl gEftAuraTaskClass
gEftAuraTaskClass:
    .long EftAuraTask_Update
    .long EftAuraTask_Init
    .long EftAuraTask_Term
    .long EftAuraTask_PostUpdate
    .long EftAuraTask_Reset
    .long EftAuraTask_Draw
.globl D_002C3A60
D_002C3A60:
    .long EftBoltMgr_Update
    .long EftBoltMgr_Init
    .long EftBoltMgr_Term
    .space 4
    .long EftBoltMgr_Reset
    .space 4
.globl gEftBoltTaskClass
gEftBoltTaskClass:
    .long EftBoltTask_Update
    .long EftBoltTask_Init
    .long EftBoltTask_Term
    .long EftBoltTask_PostUpdate
    .long EftBoltTask_Reset
    .long EftBoltTask_Draw
.globl D_002C3A90
D_002C3A90:
    .long EftRaysMgr_Update
    .long EftRaysMgr_Init
    .long EftRaysMgr_Term
    .space 12
.globl gEftRaysClass
gEftRaysClass:
    .long EftRays_Update
    .long EftRays_Init
    .long EftRays_Term
    .space 4
    .long EftRays_Reset
    .long EftRays_Draw
.globl D_002C3AC0
D_002C3AC0:
    .long EftBlastObjMgr_Update
    .long EftBlastObjMgr_Init
    .long EftBlastObjMgr_Term
    .space 4
    .long EftBlastObjMgr_Reset
    .space 4
.globl gEftBlastObjClass
gEftBlastObjClass:
    .long EftBlastObj_Update
    .long EftBlastObj_Init
    .long EftBlastObj_Term
    .long EftBlastObj_PostUpdate
    .long EftBlastObj_Reset
    .long EftBlastObj_Draw
.globl D_002C3AF0
D_002C3AF0:
    .long EftBodyFxMgr_Update
    .long EftBodyFxMgr_Init
    .long EftBodyFxMgr_Term
    .space 12
.globl gEftBodyFxClass
gEftBodyFxClass:
    .long EftBodyFx_Update
    .long EftBodyFx_Init
    .long EftBodyFx_Term
    .long EftBodyFx_PostUpdate
    .long EftBodyFx_Reset
    .long EftBodyFx_Draw
.globl D_002C3B20
D_002C3B20:
    .long EftDiscMgr_Update
    .long EftDiscMgr_Init
    .long EftDiscMgr_Term
    .space 4
    .long EftDiscMgr_Reset
    .space 4
.globl D_002C3B38
D_002C3B38:
    .long EftDisc_Update
    .long EftDisc_Init
    .long EftDisc_Term
    .long EftDisc_PostUpdate
    .long EftDisc_Reset
    .long EftDisc_Draw
.globl gEftGlowMgrClass
gEftGlowMgrClass:
    .long EftGlowMgr_Update
    .long EftGlowMgr_Init
    .long EftGlowMgr_Term
    .space 12
.globl gEftGlowClass
gEftGlowClass:
    .long EftGlowTask_Update
    .long EftGlowTask_Init
    .long EftGlowTask_Term
    .long EftGlowTask_PostUpdate
    .long EftGlowTask_Reset
    .long EftGlowTask_Draw
.globl gEftRushBurstMgrClass
gEftRushBurstMgrClass:
    .long EftRushBurstMgr_Update
    .long EftRushBurstMgr_Init
    .long EftRushBurstMgr_Term
    .space 12
.globl gEftRushBurstClass
gEftRushBurstClass:
    .long EftRushBurst_Update
    .long EftRushBurst_Init
    .long EftRushBurst_Term
    .long EftRushBurst_PostUpdate
    .long EftRushBurst_Reset
    .long EftRushBurst_Draw
.globl gEftCharRootClass
gEftCharRootClass:
    .long EftCharRoot_Update
    .long EftCharRoot_Init
    .long EftCharRoot_Term
    .space 12
.globl gEftCharClass
gEftCharClass:
    .long EftChar_Update
    .long EftChar_Init
    .long EftChar_Term
    .space 12
.globl gEftCharKindClass
gEftCharKindClass:
    .long gEftChargeMgrClass
    .long gEftKiBlastMgrClass
    .long gEftKiBlastMgrClass
    .long gEftBlastChargeMgrClass
    .long gEftShockMgrClass
    .long gEftRushBurstMgrClass
    .long gEftCharNullMgrClass
    .long gEftCharaFxMgrClass
    .long gEftCharNullMgrClass
    .long gEftAbsorbMgrClass
    .long gEftCharNullMgrClass
    .space 4
.globl gEftCharNullMgrClass
gEftCharNullMgrClass:
    .long EftCharNullMgr_Update
    .long EftCharNullMgr_Init
    .long EftCharNullMgr_Term
    .space 12
.globl gEftCharNullClass
gEftCharNullClass:
    .long EftCharNull_Update
    .long EftCharNull_Init
    .long EftCharNull_Term
    .long EftCharNull_PostUpdate
    .long EftCharNull_Reset
    .long EftCharNull_Draw
.globl gEftFlashMgrClass
gEftFlashMgrClass:
    .long EftFlashMgr_Update
    .long EftFlashMgr_Init
    .long EftFlashMgr_Term
    .space 4
    .long EftFlashMgr_Reset
    .space 4
.globl gEftFlashClass
gEftFlashClass:
    .long EftFlash_Update
    .long EftFlash_Init
    .long EftFlash_Term
    .space 4
    .long EftFlash_Reset
    .long EftFlash_Draw
.globl gEftTrailMgrClass
gEftTrailMgrClass:
    .long EftTrailMgr_Update
    .long EftTrailMgr_Init
    .long EftTrailMgr_Term
    .space 12
.globl gEftTrailClass
gEftTrailClass:
    .long EftTrail_Update
    .long EftTrail_Init
    .long EftTrail_Term
    .space 4
    .long EftTrail_Reset
    .long EftTrail_Draw
.globl gEftTrailOrigin
gEftTrailOrigin:
    .space 16
.globl gEftCharaFxMgrClass
gEftCharaFxMgrClass:
    .long EftCharaFxMgr_Update
    .long EftCharaFxMgr_Init
    .long EftCharaFxMgr_Term
    .space 12
.globl gEftCharaFxClass
gEftCharaFxClass:
    .long EftCharaFx_Update
    .long EftCharaFx_Init
    .long EftCharaFx_Term
    .long EftCharaFx_PostUpdate
    .long EftCharaFx_Reset
    .long EftCharaFx_Draw
.globl gEftStruggleMgrClass
gEftStruggleMgrClass:
    .long EftStruggleMgr_Update
    .long EftStruggleMgr_Init
    .long EftStruggleMgr_Term
    .space 12
.globl gEftStruggleClass
gEftStruggleClass:
    .long EftStruggle_Update
    .long EftStruggle_Init
    .long EftStruggle_Term
    .space 4
    .long EftStruggle_Reset
    .long EftStruggle_Draw
.globl gEftClashSparkMgrClass
gEftClashSparkMgrClass:
    .long EftClashSparkMgr_Update
    .long EftClashSparkMgr_Init
    .long EftClashSparkMgr_Term
    .space 4
    .long EftClashSparkMgr_Reset
    .space 4
.globl gEftClashSparkClass
gEftClashSparkClass:
    .long EftClashSpark_Update
    .long EftClashSpark_Init
    .long EftClashSpark_Term
    .long EftClashSpark_PostUpdate
    .long EftClashSpark_Reset
    .long EftClashSpark_Draw
.globl gEftChargeMgrClass
gEftChargeMgrClass:
    .long EftChargeMgr_Update
    .long EftChargeMgr_Init
    .long EftChargeMgr_Term
    .space 12
.globl gEftChargeClass
gEftChargeClass:
    .long EftCharge_Update
    .long EftCharge_Init
    .long EftCharge_Term
    .long EftCharge_PostUpdate
    .long EftCharge_Reset
    .long EftCharge_Draw
.globl gEftKiBlastMgrClass
gEftKiBlastMgrClass:
    .long EftKiBlastMgr_Update
    .long EftKiBlastMgr_Init
    .long EftKiBlastMgr_Term
    .space 12
.globl gEftKiBlastClass
gEftKiBlastClass:
    .long EftKiBlast_Update
    .long EftKiBlast_Init
    .long EftKiBlast_Term
    .long EftKiBlast_PostUpdate
    .long EftKiBlast_Reset
    .long EftKiBlast_Draw
.globl gEftBlastChargeMgrClass
gEftBlastChargeMgrClass:
    .long EftBlastChargeMgr_Update
    .long EftBlastChargeMgr_Init
    .long EftBlastChargeMgr_Term
    .space 12
.globl gEftBlastChargeClass
gEftBlastChargeClass:
    .long EftBlastCharge_Update
    .long EftBlastCharge_Init
    .long EftBlastCharge_Term
    .long EftBlastCharge_PostUpdate
    .long EftBlastCharge_Reset
    .long EftBlastCharge_Draw
.globl gEftKiBombMgrClass
gEftKiBombMgrClass:
    .long EftKiBombMgr_Update
    .long EftKiBombMgr_Init
    .long EftKiBombMgr_Term
    .space 12
.globl gEftKiBombClass
gEftKiBombClass:
    .long EftKiBomb_Update
    .long EftKiBomb_Init
    .long EftKiBomb_Term
    .long EftKiBomb_PostUpdate
    .long EftKiBomb_Reset
    .long EftKiBomb_Draw
.globl D_002C3E00
D_002C3E00:
    .long EftKiObjMgr_Update
    .long EftKiObjMgr_Init
    .long EftKiObjMgr_Term
    .space 12
.globl D_002C3E18
D_002C3E18:
    .long EftKiObj_Update
    .long EftKiObj_Init
    .long EftKiObj_Term
    .long EftKiObj_PostUpdateCb
    .long EftKiObj_Reset
    .long EftKiObj_DrawCb
.globl D_002C3E30
D_002C3E30:
    .long EftChainMgr_Update
    .long EftChainMgr_Init
    .long EftChainMgr_Term
    .space 4
    .long EftChainMgr_Reset
    .space 4
.globl D_002C3E48
D_002C3E48:
    .long EftChain_Update
    .long EftChain_Init
    .long EftChain_Term
    .long EftChain_PostUpdate
    .long EftChain_Reset
    .long EftChain_Draw
.globl gEftRayMgrClass
gEftRayMgrClass:
    .long EftRayMgr_Update
    .long EftRayMgr_Init
    .long EftRayMgr_Term
    .space 12
.globl gEftRayClass
gEftRayClass:
    .long EftRay_Update
    .long EftRay_Init
    .long EftRay_Term
    .space 4
    .long EftRay_Reset
    .long EftRay_Draw
.globl gEftStreakMgrClass
gEftStreakMgrClass:
    .long EftStreakMgr_Update
    .long EftStreakMgr_Init
    .long EftStreakMgr_Term
    .space 4
    .long EftStreakMgr_Reset
    .space 4
.globl gEftStreakClass
gEftStreakClass:
    .long EftStreak_Update
    .long EftStreak_Init
    .long EftStreak_Term
    .long EftStreak_PostUpdate
    .long EftStreak_Reset
    .long EftStreak_Draw
.globl D_002C3EC0
D_002C3EC0:
    .long EftShotFx_Update
    .long EftShotFxMgr_Init
    .long EftShotFx_Term
    .long EftShotFx_PostUpdate
    .space 4
    .long EftShotFx_Draw
.globl gEftPtclMgrClass
gEftPtclMgrClass:
    .long EftPtclMgr_Update
    .long EftPtclMgr_Init
    .long EftPtclMgr_Term
    .space 4
    .long EftPtclMgr_Reset
    .space 4
.globl gEftPtclClass
gEftPtclClass:
    .long EftPtcl_Update
    .long EftPtcl_Init
    .long EftPtcl_Term
    .long EftPtcl_PostUpdate
    .long EftPtcl_Reset
    .long EftPtcl_Draw
.globl gEftImpactMgrClass
gEftImpactMgrClass:
    .long EftImpactMgr_Update
    .long EftImpactMgr_Init
    .long EftImpactMgr_Term
    .space 4
    .long EftImpactMgr_Reset
    .space 4
.globl gEftImpactClass
gEftImpactClass:
    .long EftImpact_Update
    .long EftImpact_Init
    .long EftImpact_Term
    .long EftImpact_PostUpdate
    .long EftImpact_Reset
    .long EftImpact_Draw
.globl gEftLinkMgrClass
gEftLinkMgrClass:
    .long EftLinkMgr_Update
    .long EftLinkMgr_Init
    .long EftLinkMgr_Term
    .space 4
    .long EftLinkMgr_Reset
    .space 4
.globl gEftLinkClass
gEftLinkClass:
    .long EftLink_Update
    .long EftLink_Init
    .long EftLink_Term
    .long EftLink_PostUpdate
    .long EftLink_Reset
    .long EftLink_Draw
.globl gEftPart10MgrClass
gEftPart10MgrClass:
    .long EftPart10Mgr_Update
    .long EftPart10Mgr_Init
    .long EftPart10Mgr_Term
    .space 4
    .long EftPart10Mgr_Reset
    .space 4
.globl gEftPart10Class
gEftPart10Class:
    .long EftPart10_Update
    .long EftPart10_Init
    .long EftPart10_Term
    .long EftPart10_PostUpdate
    .long EftPart10_Reset
    .long EftPart10_Draw
.globl gEftLayer3Class
gEftLayer3Class:
    .long EftLayer3_Update
    .long EftLayer3_Init
    .long EftLayer3_Term
    .space 4
    .long EftLayer3_Reset
    .space 4
.globl gEftLayer3Classes
gEftLayer3Classes:
    .long D_002C4108
    .space 4
    .long gEftAuraMgrClass
    .space 4
    .long D_002C3A60
    .space 4
    .long gEftGlowMgrClass
    .space 4
    .long D_002C3A18
    .space 4
    .long D_002C3EC0
    .space 4
    .long gEftBladeMgrClass
    .space 4
    .long gEftStruggleMgrClass
    .space 4
    .long D_002C3B20
    .space 4
    .long gEftOrbTailMgrClass
    .space 4
    .long gEftStreakMgrClass
    .space 4
    .long gEftCharSlotMgrClass
    .space 4
    .long gEftTransformMgrClass
    .space 4
    .long gEftImpactMgrClass
    .space 4
    .long D_002C3AF0
    .space 4
    .long D_002C3AC0
    .space 4
    .long gEftClashSparkMgrClass
    .space 4
    .long D_002C3A90
    .space 4
    .long gEftAnimPartMgrClass
    .space 4
    .long gEftRibbonMgrClass
    .space 4
    .long gEftPtclMgrClass
    .space 4
    .long D_002C3E30
    .space 4
    .long gEftQuadMgrClass
    .space 4
    .long gEftPart10MgrClass
    .space 4
    .long gEftLinkMgrClass
    .space 4
    .long D_002C40D8
    .space 4
    .long gEftZapMgrClass
    .space 4
    .long gEftRayMgrClass
    .space 4
    .long gEftBlindClass
    .space 4
    .long gEftFlashMgrClass
    .space 4
    .long gEftDelaySeMgrClass
    .space 4
.globl gEftQuadMgrClass
gEftQuadMgrClass:
    .long EftQuadMgr_Update
    .long EftQuadMgr_Init
    .long EftQuadMgr_Term
    .space 4
    .long EftQuadMgr_Reset
    .space 4
.globl gEftQuadClass
gEftQuadClass:
    .long EftQuad_Update
    .long EftQuad_Init
    .long EftQuad_Term
    .long EftQuad_PostUpdate
    .long EftQuad_Reset
    .long EftQuad_Draw
.globl D_002C40D8
D_002C40D8:
    .long EftBillMgr_Update
    .long EftBillMgr_Init
    .long EftBillMgr_Term
    .space 4
    .long EftBillMgr_Reset
    .space 4
.globl D_002C40F0
D_002C40F0:
    .long EftBill_Update
    .long EftBill_Init
    .long EftBill_Term
    .long EftBill_PostUpdate
    .long EftBill_Reset
    .long EftBill_Draw
.globl D_002C4108
D_002C4108:
    .long EftGndDustMgr_Update
    .long EftGndDustMgr_Init
    .long EftGndDustMgr_Term
    .space 12
.globl D_002C4120
D_002C4120:
    .long EftGndDustPuff_Update
    .long EftGndDustPuff_Init
    .long EftGndDustPuff_Term
    .space 4
    .long EftGndDustPuff_Reset
    .long EftGndDustPuff_Draw
.globl D_002C4138
D_002C4138:
    .long EftGndDustSlide_Update
    .long EftGndDustSlide_Init
    .long EftGndDustSlide_Term
    .space 4
    .long EftGndDustSlide_Reset
    .long EftGndDustSlide_Draw
.globl D_002C4150
D_002C4150:
    .long EftGndDustDash_Update
    .long EftGndDustDash_Init
    .long EftGndDustDash_Term
    .space 4
    .long EftGndDustDash_Reset
    .long EftGndDustDash_Draw
.globl D_002C4168
D_002C4168:
    .long EftGndDustBurst_Update
    .long EftGndDustBurst_Init
    .long EftGndDustBurst_Term
    .space 4
    .long EftGndDustBurst_Reset
    .long EftGndDustBurst_Draw
.globl D_002C4180
D_002C4180:
    .long EftGndDustLand_Update
    .long EftGndDustLand_Init
    .long EftGndDustLand_Term
    .space 4
    .long EftGndDustLand_Reset
    .long EftGndDustLand_Draw
.globl D_002C4198
D_002C4198:
    .long EftGndDustImpact_Update
    .long EftGndDustImpact_Init
    .long EftGndDustImpact_Term
    .space 4
    .long EftGndDustImpact_Reset
    .long EftGndDustImpact_Draw
.globl gEftDelaySeMgrClass
gEftDelaySeMgrClass:
    .long EftDelaySeMgr_Update
    .long EftDelaySeMgr_Init
    .long EftDelaySeMgr_Term
    .space 12
.globl gEftDelaySeClass
gEftDelaySeClass:
    .long EftDelaySe_Update
    .long EftDelaySe_Init
    .long EftDelaySe_Term
    .space 4
    .long EftDelaySe_Reset
    .space 4
.globl gEftCharSlotMgrClass
gEftCharSlotMgrClass:
    .long EftCharSlotMgr_Update
    .long EftCharSlotMgr_Init
    .long EftCharSlotMgr_Term
    .space 4
    .long EftCharSlotMgr_Reset
    .space 4
.globl gEftBladeColor
gEftBladeColor:
    .space 32
.globl gEftBladeMgrClass
gEftBladeMgrClass:
    .long EftBladeMgr_Update
    .long EftBladeMgr_Init
    .long EftBladeMgr_Term
    .space 12
.globl gEftBladeClass
gEftBladeClass:
    .long EftBlade_Update
    .long EftBlade_Init
    .long EftBlade_Term
    .space 4
    .long EftBlade_Reset
    .long EftBlade_Draw
.globl gEftBlindClass
gEftBlindClass:
    .long EftBlind_Update
    .long EftBlind_Init
    .long EftBlind_Term
    .space 8
    .long EftBlind_Draw
.globl gEftAnimPartMgrClass
gEftAnimPartMgrClass:
    .long EftAnimPartMgr_Update
    .long EftAnimPartMgr_Init
    .long EftAnimPartMgr_Term
    .space 12
.globl gEftAnimPartClass
gEftAnimPartClass:
    .long EftAnimPart_Update
    .long EftAnimPart_Init
    .long EftAnimPart_Term
    .space 4
    .long EftAnimPart_Reset
    .long EftAnimPart_Draw
.globl gEftOrbTailMgrClass
gEftOrbTailMgrClass:
    .long EftOrbTailMgr_Update
    .long EftOrbTailMgr_Init
    .long EftOrbTailMgr_Term
    .space 4
    .long EftOrbTailMgr_Reset
    .space 4
.globl gEftOrbTailClass
gEftOrbTailClass:
    .long EftOrbTail_Update
    .long EftOrbTail_Init
    .long EftOrbTail_Term
    .long EftOrbTail_PostUpdate
    .long EftOrbTail_Reset
    .long EftOrbTail_Draw
.globl gEftTransformMgrClass
gEftTransformMgrClass:
    .long EftTransformMgr_Update
    .long EftTransformMgr_Init
    .long EftTransformMgr_Term
    .space 12
.globl gEftTransformClass
gEftTransformClass:
    .long EftTransform_Update
    .long EftTransform_Init
    .long EftTransform_Term
    .long EftTransform_PostUpdate
    .long EftTransform_Reset
    .long EftTransform_Draw
.globl gEftRibbonMgrClass
gEftRibbonMgrClass:
    .long EftRibbonMgr_Update
    .long EftRibbonMgr_Init
    .long EftRibbonMgr_Term
    .space 4
    .long EftRibbonMgr_Reset
    .space 4
.globl gEftRibbonClass
gEftRibbonClass:
    .long EftRibbon_Update
    .long EftRibbon_Init
    .long EftRibbon_Term
    .long EftRibbon_PostUpdate
    .long EftRibbon_Reset
    .long EftRibbon_Draw
.globl gEftZapMgrClass
gEftZapMgrClass:
    .long EftZapMgr_Update
    .long EftZapMgr_Init
    .long EftZapMgr_Term
    .space 4
    .long EftZapMgr_Reset
    .long EftZapMgr_Draw
.globl gEftZapClass
gEftZapClass:
    .long EftZap_Update
    .long EftZap_Init
    .long EftZap_Term
    .long EftZap_PostUpdate
    .long EftZap_Reset
    .long EftZap_Draw
.globl gEftShockMgrClass
gEftShockMgrClass:
    .long EftShockMgr_Update
    .long EftShockMgr_Init
    .long EftShockMgr_Term
    .space 12
.globl gEftShockClass
gEftShockClass:
    .long EftShock_Update
    .long EftShock_Init
    .long EftShock_Term
    .long EftShock_PostUpdate
    .long EftShock_Reset
    .long EftShock_Draw
.globl gEftSprUvScale
gEftSprUvScale:
    .space 16
.globl gEftSprGridUv2x2
gEftSprGridUv2x2:
    .space 64
.globl gEftSprGridUv4x2
gEftSprGridUv4x2:
    .space 128
.globl gEftSprGridUv3x3
gEftSprGridUv3x3:
    .space 144
.globl gEftSprGridUv4x4
gEftSprGridUv4x4:
    .space 256
.globl gEftSprSubdivCorner
gEftSprSubdivCorner:
    .space 64
.globl D_002C4620
D_002C4620:
    .space 64
.globl D_002C4660
D_002C4660:
    .space 80
.globl gEftVolleyAimScripts
gEftVolleyAimScripts:
    .long D_002C4620
    .long D_002C4660
.globl gBtlAiStateFuncs
gBtlAiStateFuncs:
    .long BtlAiSeq_PhaseInit
    .long BtlAiSeq_PhaseStart
    .long BtlAiSeq_PhaseRun
    .long BtlAiSeq_PhaseEnd
.globl gBtlAiPickFuncs
gBtlAiPickFuncs:
    .long BtlAiPick_Random
    .long BtlAiPick_ByArg
    .long BtlAiPick_ArgSeconds
    .long BtlAiPick_ArgFrames
    .long BtlAiPick_KiTarget
    .long BtlAiPick_SkillCharge
    .long BtlAiPick_LevelDelay
    .long BtlAiPick_ComboBranch
    .long BtlAiPick_FusionSlot
    .long BtlAiPick_TransformSlot
    .long BtlAiPick_SwitchMember
    .long BtlAiPick_DummyMode
    .long BtlAiPick_PromptButton
    .long BtlAiPick_GuardMode
    .long BtlAiPick_Direction
    .long BtlAiPick_SetTimer
.globl gBtlAiStepFuncs
gBtlAiStepFuncs:
    .long BtlAiStep_ReachClass
    .long BtlAiStep_ChargeKi
    .long BtlAiStep_UntilBlocked
    .long BtlAiStep_Countdown
    .long BtlAiStep_BlockedOrCharged
    .long BtlAiStep_BlockedAndCharged
    .long BtlAiStep_NotHit
    .long BtlAiStep_Charged
    .long BtlAiStep_GuardUntilSafe
    .long BtlAiStep_GuardCountdown
    .long BtlAiStep_ReachClassNoShots
    .long BtlAiStep_OneFrame
    .long BtlAiStep_FacingOpponent
    .long BtlAiStep_SwitchQueued
    .long BtlAiStep_Unk14
    .long BtlAiStep_Unk15
    .long BtlAiStep_WaitNear2
    .long BtlAiStep_FireSkill
    .long BtlAiStep_Unk18
    .long BtlAiStep_Unk19
    .long BtlAiStep_Unk20
    .long BtlAiStep_Unk21
    .long BtlAiStep_WaitNear3
    .long BtlAiStep_Unk23
.globl D_002C4768
D_002C4768:
    .long BtlAiCond_False
    .long BtlAiCond_True
    .long BtlAiCond_Percent
    .long BtlAiCond_Idle
    .long BtlAiCond_LevelAtLeast
    .long BtlAiCond_LevelBelow
    .long BtlAiCond_Status4
    .long BtlAiCond_GaugeAPercent
    .long BtlAiCond_GaugeBStock
    .long BtlAiCond_Flag6
    .long BtlAiCond_Status6
    .long BtlAiCond_Unk11
    .long BtlAiCond_Unk12
    .long BtlAiCond_Plan8Is2
    .long BtlAiCond_WorkBit0
    .long BtlAiCond_WorkBit1
    .long BtlAiCond_WorkBit2
    .long BtlAiCond_SetPlan54
    .long BtlAiCond_SeqFlag2
    .long BtlAiCond_LevelBelowDummy
    .long BtlAiCond_React
    .long BtlAiCond_SeqReady
    .long BtlAiCond_Stage4Or27
    .long BtlAiCond_React40000
    .long BtlAiCond_Unk24
    .long BtlAiCond_Rate0
    .long BtlAiCond_Rate5
    .long BtlAiCond_Unk27
    .long BtlAiCond_Rate6
    .long BtlAiCond_Rate1
    .long BtlAiCond_Rate4
    .long BtlAiCond_TypeRateByOppAction
    .long BtlAiCond_Rate2
    .long BtlAiCond_Rate8
    .long BtlAiCond_TenPercent
    .long AiThCond_Basic
    .long AiThCond_Basic
    .long AiThCond_Basic
    .long AiThCond_BasicNoFlag6
    .long AiThCond_Basic
    .long AiThCond_WeightedIfBit0
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_WeightedByHeight
    .long AiThCond_WeightedByHeight
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_ActRateByFlags
    .long AiThCond_MiscRate0Or3
    .long AiThCond_MiscRate1
    .long AiThCond_MiscRate2
    .long AiThCond_MiscRate4
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_WeightedByHeight
    .long AiThCond_WeightedByHeight
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Move
    .long AiThCond_Skill
    .long AiThCond_Skill
    .long AiThCond_Skill
    .long AiThCond_Skill
    .long AiThCond_Skill
    .long AiThCond_Skill
    .long AiThCond_Skill
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Skill
    .long AiThCond_Skill
    .long AiThCond_React4000
    .long AiThCond_React8000
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_Weighted
    .long AiThCond_WeightedIfAbility
    .long AiThCond_WeightedIfAbility
    .long AiThCond_Weighted
    .long AiThCond_WeightedIfAbility
.globl gBtlAiMovePhases
gBtlAiMovePhases:
    .long BtlAiMove_Init
    .long BtlAiMove_Start
    .long BtlAiMove_Run
    .long BtlAiMove_End
.globl gBtlAiComboPhases
gBtlAiComboPhases:
    .long BtlAiCombo_Init
    .long BtlAiCombo_Start
    .long BtlAiAtk_Run
    .long BtlAiAtk_End
.globl gBtlAiFollowPhases
gBtlAiFollowPhases:
    .long BtlAiFollow_Init
    .long BtlAiFollow_Start
    .long BtlAiAtk_Run
    .long BtlAiFollow_End
.globl gBtlAiAct36Phases
gBtlAiAct36Phases:
    .long BtlAiAct36_Init
    .long BtlAiAct36_Start
    .long BtlAiAtk_Run
    .long BtlAiAtk_End
.globl gBtlActHandlers
gBtlActHandlers:
    .space 4
    .long BtlAct_Action01
    .long BtlAct_Action02
    .long BtlAct_Action03
    .long BtlAct_Action04
    .long BtlAct_Action05
    .long BtlAct_Action05
    .long BtlAct_WaitHandler
    .long BtlAct_WaitHandler
    .long BtlAct_WaitHandler
    .long BtlAct_WaitHandler
    .long BtlAct_NeutralHandler
    .long BtlAct_IdleOnceHandler
    .long BtlAct_MoveHandler
    .long BtlAct_CloseMoveHandler
    .long BtlAct_DashMoveHandler
    .long BtlAct_JumpStartHandler
    .long BtlAct_JumpAirHandler
    .long BtlAct_JumpStartHandler
    .long BtlAct_JumpAirHandler
    .long BtlAct_AscendHandler
    .long BtlAct_DescendHandler
    .long BtlAct_HopHandler
    .long BtlAct_FastAscendHandler
    .long BtlAct_FastDescendHandler
    .long BtlAct_HomingDashHandler
    .long BtlAct_DragonDashHandler
    .long BtlAct_StepHandler
    .long BtlAct_StepHandler
    .long BtlAct_StepHandler
    .long BtlAct_StepInHandler
    .long BtlAct_StepInHandler
    .long BtlAct_VanishStepHandler
    .long BtlAct_VanishStepHandler
    .long BtlAct_VanishStepHandler
    .long BtlAct_VanishStepHandler
    .long BtlAct_VanishBehindHandler
    .long BtlAct_RecoverHandler
    .long BtlAct_RecoverHandler
    .long BtlAct_RecoverHandler
    .long BtlAct_RecoverHandler
    .long BtlAct_RecoverHandler
    .long BtlAct_RecoverHandler
    .long BtlAct_VanishAttackHandler
    .long BtlAct_VanishAttackQuickHandler
    .long BtlAct_WarpBehindAttackHandler
    .long BtlAct_WarpAheadAttackHandler
    .long BtlAct_SlideToAttackHandler
    .long BtlAct_SlideToAttackHandler
    .long BtlAct_RushToAttackHandler
    .long BtlAct_HopBackAttackHandler
    .long BtlAct_CircleDashHandler
    .long BtlAct_RushDashHandler
    .long BtlAct_VanishDashHandler
    .long BtlAct_SearchHandler
    .long BtlAct_ChargeHandler
    .long BtlAct_GuardHandler
    .long BtlAct_Action39
    .long BtlAct_Action3A
    .long BtlAct_Action3B
    .long BtlAct_Action3C
    .long BtlAct_Action3D
    .long BtlAct_Action3E
    .long BtlAct_Action3F
    .long BtlAct_Action40
    .long BtlAct_Action41
    .long BtlAct_Action42
    .long BtlAct_Action43
    .long BtlAct_RushHandler
    .long BtlAct_RushAutoHandler
    .long BtlAct_RushRapidHandler
    .long BtlAct_SmashChargeHandler
    .long BtlAct_SmashChargeHandler
    .long BtlAct_SmashChargeHandler
    .long BtlAct_SmashChargeHandler
    .long BtlAct_SmashChargeHandler
    .long BtlAct_SmashChargeHandler
    .long BtlAct_SmashFullHandler
    .long BtlAct_SmashFullHandler
    .long BtlAct_SmashFullHandler
    .long BtlAct_SmashFullHandler
    .long BtlAct_SmashFullHandler
    .long BtlAct_SmashFullHandler
    .long BtlAct_DashSmashHandler
    .long BtlAct_DashSmashHandler
    .long BtlAct_DashSmashHandler
    .long BtlAct_DashSmashHandler
    .long BtlAct_DashSmashHandler
    .long BtlAct_FlyingKickHandler
    .long BtlAct_LiftStrikeHandler
    .long BtlAct_SmashVanishHandler
    .long BtlAct_SmashVanishHandler
    .long BtlAct_SmashVanishHandler
    .long BtlAct_SmashVanishHandler
    .long BtlAct_RushFinishHandler
    .long BtlAct_RushFinishHandler
    .long BtlAct_RushFinishHandler
    .long BtlAct_RushFinishHandler
    .long BtlAct_RushFinishHandler
    .long BtlAct_Action63
    .long BtlAct_RushFinishHandler
    .long BtlAct_RushFinishHandler
    .long BtlAct_RushFinishHandler
    .long BtlAct_Action67to69
    .long BtlAct_Action67to69
    .long BtlAct_Action67to69
    .long BtlAct_Action6A
    .long BtlAct_Action6Bto6F
    .long BtlAct_Action6Bto6F
    .long BtlAct_Action6Bto6F
    .long BtlAct_Action6Bto6F
    .long BtlAct_Action6Bto6F
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackDashHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_KiVolley95
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackVolleyDashHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackDashHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackDashHandler
    .long BtlAct_AttackHandler
    .long BtlAct_AttackHandler
    .long BtlAct_KiBlastHandler
    .long BtlAct_ChargedKiBlastHandler
    .long BtlAct_DashKiBlastHandler
    .long BtlAct_DashChargedKiBlastHandler
    .long BtlAct_KiBlastB2
    .long BtlAct_KiBlastChargeB3
    .long BtlAct_GrabDash
    .long BtlAct_GrabDash
    .long BtlAct_GrabDash
    .long BtlAct_Throw
    .long BtlAct_Thrown
    .long BtlAct_Throw
    .long BtlAct_Thrown
    .long BtlAct_SlamThrow
    .long BtlAct_SlamThrown
    .long BtlAct_HitStaggerHandler
    .long BtlAct_DragDown
    .long BtlAct_DraggedDown
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_HitStaggerHandler
    .long BtlAct_ActionCBtoCC
    .long BtlAct_ActionCBtoCC
    .long BtlAct_GroundBounceHandler
    .long BtlAct_LaunchedHandler
    .long BtlAct_ActionCFtoD1
    .long BtlAct_ActionCFtoD1
    .long BtlAct_ActionCFtoD1
    .long BtlAct_ActionD2
    .long BtlAct_StunHandler
    .long BtlAct_KnockBackHandler
    .long BtlAct_BlowAwayHandler
    .long BtlAct_BlowAwayHandler
    .long BtlAct_BlowRecoverHandler
    .long BtlAct_LieDownHandler
    .long BtlAct_FallHandler
    .long BtlAct_DownHandler
    .long BtlAct_DownHandler
    .long BtlAct_DownHandler
    .long BtlAct_DownHandler
    .long BtlAct_DownHandler
    .long BtlAct_BlowAwayHandler
    .long BtlAct_AirStunHandler
    .long BtlAct_GetUpHandler
    .long BtlAct_GetUpHandler
    .long BtlAct_GetUpHandler
    .long BtlAct_GetUpHandler
    .long BtlAct_GetUpHandler
    .long BtlAct_ActionE6
    .long BtlAct_ActionE7
    .long BtlAct_ActionE8E9
    .long BtlAct_ActionE8E9
    .long BtlAct_ActionEA
    .long BtlAct_ActionEB
    .long BtlAct_TransformA
    .long BtlAct_TransformB
    .long BtlAct_TransformC
    .long BtlAct_TransformD
    .long BtlAct_TransformE
    .long BtlAct_Fusion
    .long BtlAct_Fusion
    .long BtlAct_SwitchLeave
    .long BtlAct_SwitchArriveWait
    .long BtlAct_SwitchArriveLand
    .long BtlAct_KoSwitchLoad
    .long BtlAct_KoSwitchEnterWait
    .long BtlAct_KoSwitchFlyIn
    .long BtlAct_RoundReset
    .long BtlAct_ClashBlowsHandler
    .long BtlAct_ClashVanishHandler
    .long BtlAct_ClashExchangeHandler
    .long BtlAct_SkillBasic
    .long BtlAct_SkillBasic
    .long BtlAct_SkillTeleport
    .long BtlAct_SkillTeleport
    .long BtlAct_SkillInstant
    .long BtlAct_SkillInstant
    .long BtlAct_ChangeRandomCharaHandler
    .long BtlAct_ClashLostHandler
    .long BtlAct_UltimateStartHandler
    .long BtlAct_SuperBeamHandler
    .long BtlAct_SuperBeamHandler
    .long BtlAct_SuperBeamHandler
    .long BtlAct_SuperWarpBeamHandler
    .long BtlAct_SuperWarpBeamHandler
    .long BtlAct_SuperWarpBeamHandler
    .long BtlAct_SuperLongBeamHandler
    .long BtlAct_SuperLongBeamHandler
    .long BtlAct_SuperLongBeamHandler
    .long BtlAct_SuperChargeHandler
    .long BtlAct_SuperChargeHandler
    .long BtlAct_SuperChargeHandler
    .long BtlAct_SuperRepeatHandler
    .long BtlAct_SuperRepeatHandler
    .long BtlAct_SuperRepeatHandler
    .long BtlAct_SuperQuickBeamHandler
    .long BtlAct_SuperQuickBeamHandler
    .long BtlAct_SuperQuickBeamHandler
    .long BtlAct_SuperQuickLongBeamHandler
    .long BtlAct_SuperQuickLongBeamHandler
    .long BtlAct_SuperQuickLongBeamHandler
    .long BtlAct_SuperRushDashHandler
    .long BtlAct_SuperRushDashHandler
    .long BtlAct_SuperRushDashHandler
    .long BtlAct_SuperRushDashHandler
    .long BtlAct_SuperRushDashHandler
    .long BtlAct_SuperRushDashHandler
    .long BtlAct_SuperRushStrikeHandler
    .long BtlAct_SuperRushStrikeHandler
    .long BtlAct_SuperRushStrikeHandler
    .long BtlAct_SuperRushFlyHandler
    .long BtlAct_SuperRushFlyHandler
    .long BtlAct_SuperRushFlyHandler
    .long BtlAct_SuperRushFollowHandler
    .long BtlAct_SuperRushFollowHandler
    .long BtlAct_SuperRushFollowHandler
    .long BtlAct_SuperRushFinishHandler
    .long BtlAct_SuperRushFinishHandler
    .long BtlAct_SuperRushFinishHandler
    .long BtlAct_SuperRushSequenceHandler
    .long BtlAct_SuperRushSequenceHandler
    .long BtlAct_SuperRushSequenceHandler
    .long BtlAct_ClashStruggleHandler
    .long BtlAct_ClashStruggleHandler
    .long BtlAct_ClashStruggleHandler
    .long BtlAct_SuperRushCaughtHandler
    .long BtlAct_SuperRushCaughtHandler
    .long BtlAct_SuperRushCaughtHandler
    .long BtlAct_SuperLaunchedUpHandler
    .long BtlAct_SuperLaunchedUpHandler
    .long BtlAct_SuperLaunchedUpHandler
    .long BtlAct_SuperRushSequenceHandler
    .long BtlAct_SuperRushSequenceHandler
    .long BtlAct_SuperRushSequenceHandler
.globl D_002C4E70
D_002C4E70:
    .space 144
.globl D_002C4F00
D_002C4F00:
    .long D_002C4E70
    .space 20
.globl gPauseMenuConfirmCursor
gPauseMenuConfirmCursor:
    .space 32
.globl D_002C4F38
D_002C4F38:
    .space 48
.globl D_002C4F68
D_002C4F68:
    .long D_002C4F38
    .space 52
.globl D_002C4FA0
D_002C4FA0:
    .space 48
.globl D_002C4FD0
D_002C4FD0:
    .long D_002C4FA0
    .space 52
.globl D_002C5008
D_002C5008:
    .space 336
.globl D_002C5158
D_002C5158:
    .long D_002C5008
    .space 20
.globl gPauseMenuCpuLevelCursor
gPauseMenuCpuLevelCursor:
    .space 32
.globl D_002C5190
D_002C5190:
    .space 104
    .long D_002C4F68
    .long PauseMenu_SkillListFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuVersus
gPauseMenuVersus:
    .long D_002C5190
    .space 52
.globl D_002C52B8
D_002C52B8:
    .space 104
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuResultVersus
gPauseMenuResultVersus:
    .long D_002C52B8
    .space 52
.globl D_002C5440
D_002C5440:
    .space 152
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuReplayRec
gPauseMenuReplayRec:
    .long D_002C5440
    .space 52
.globl D_002C5598
D_002C5598:
    .space 152
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuResultReplayRec
gPauseMenuResultReplayRec:
    .long D_002C5598
    .space 52
.globl D_002C5720
D_002C5720:
    .space 104
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuReplayPlay
gPauseMenuReplayPlay:
    .long D_002C5720
    .space 52
.globl D_002C57E8
D_002C57E8:
    .space 104
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuResultReplayPlay
gPauseMenuResultReplayPlay:
    .long D_002C57E8
    .space 52
.globl D_002C58B0
D_002C58B0:
    .space 104
    .long D_002C4F68
    .long PauseMenu_SkillListFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuMode1
gPauseMenuMode1:
    .long D_002C58B0
    .space 52
.globl D_002C59A8
D_002C59A8:
    .space 56
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuResultMode1
gPauseMenuResultMode1:
    .long D_002C59A8
    .space 52
.globl D_002C5A40
D_002C5A40:
    .space 104
    .long D_002C4F68
    .long PauseMenu_SkillListFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuMode2
gPauseMenuMode2:
    .long D_002C5A40
    .space 52
.globl D_002C5B38
D_002C5B38:
    .space 104
    .long D_002C4F68
    .long PauseMenu_SkillListFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuMode3
gPauseMenuMode3:
    .long D_002C5B38
    .space 52
.globl D_002C5C30
D_002C5C30:
    .space 104
    .long D_002C4F68
    .long PauseMenu_SkillListFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuMode4
gPauseMenuMode4:
    .long D_002C5C30
    .space 52
.globl D_002C5D58
D_002C5D58:
    .space 104
    .long D_002C4F68
    .long PauseMenu_SkillListFunc
    .space 40
    .long D_002C4FD0
    .space 44
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuMode5
gPauseMenuMode5:
    .long D_002C5D58
    .space 52
.globl D_002C5E80
D_002C5E80:
    .space 104
    .long D_002C4F68
    .long PauseMenu_SkillListFunc
    .space 40
    .long D_002C5158
    .long PauseMenu_CpuLevelFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 40
    .long D_002C4F00
    .long PauseMenu_ConfirmFunc
    .space 32
.globl gPauseMenuMode6
gPauseMenuMode6:
    .long D_002C5E80
    .space 52
.globl D_002C6008
D_002C6008:
    .space 48
.globl gPauseMenuMode7
gPauseMenuMode7:
    .long D_002C6008
    .space 52
.globl gBtlSeqTblDefault
gBtlSeqTblDefault:
    .long BtlSeqStageIntro_Enter
    .long BtlSeqStageIntro_PreUpdate
    .long BtlSeqStageIntro_Update
    .long BtlSeqStageIntro_Exit
    .space 4
    .long BtlPause_CheckStart
    .long BtlSeqIntroTalk_Enter
    .long BtlSeqIntroTalk_PreUpdate
    .long BtlSeqIntroTalk_Update
    .long BtlSeqIntroTalk_Exit
    .space 4
    .long BtlPause_CheckStart
    .long BtlSeqReady_Enter
    .long BtlSeqReady_PreUpdate
    .long BtlSeqReady_Update
    .long BtlSeqReady_Exit
    .space 8
    .long BtlSeqFight_Enter
    .long BtlSeqFight_PreUpdate
    .long BtlSeqFight_Update
    .long BtlSeqFight_Exit
    .space 4
    .long BtlPause_CheckOpen
    .long BtlSeqFinish_Enter
    .long BtlSeqFinish_PreUpdate
    .long BtlSeqFinish_Update
    .long BtlSeqFinish_Exit
    .space 8
    .long BtlSeqWinTalk_Enter
    .long BtlSeqWinTalk_PreUpdate
    .long BtlSeqWinTalk_Update
    .long BtlSeqWinTalk_Exit
    .space 4
    .long BtlPause_CheckStart
    .long BtlSeqEnd_Enter
    .long BtlSeqEnd_PreUpdate
    .long BtlSeqEnd_Update
    .long BtlSeqEnd_Exit
    .space 8
.globl gBtlSeqTblMode1
gBtlSeqTblMode1:
    .space 24
    .long BtlSeqIntroTalk_Enter
    .long BtlSeqIntroTalk_PreUpdate
    .long BtlSeqIntroTalk_Update
    .long BtlSeqIntroTalk_Exit
    .long BtlSeqIntroTalk_Skip
    .long BtlPause_CheckStart
    .long BtlSeqReady_Enter
    .long BtlSeqReady_PreUpdate
    .long BtlSeqReady_Update
    .long BtlSeqReady_Exit
    .space 8
    .long BtlSeqFight_Enter
    .long BtlSeqFight_PreUpdate
    .long BtlSeqFight_Update
    .long BtlSeqFight_Exit
    .space 4
    .long BtlPause_CheckOpen
    .long BtlSeqFinish_Enter
    .long BtlSeqFinish_PreUpdate
    .long BtlSeqFinish_Update
    .long BtlSeqFinish_Exit
    .space 8
    .long BtlSeqWinTalk_Enter
    .long BtlSeqWinTalk_PreUpdate
    .long BtlSeqWinTalk_Update
    .long BtlSeqWinTalk_Exit
    .long BtlSeqWinTalk_Skip
    .long BtlPause_CheckStart
    .long BtlSeqEnd_Enter
    .long BtlSeqEnd_PreUpdate
    .long BtlSeqEnd_Update
    .long BtlSeqEnd_Exit
    .space 8
.globl gBtlSeqTblMode5to7
gBtlSeqTblMode5to7:
    .space 48
    .long BtlSeqReady_Enter
    .long BtlSeqReady_PreUpdate
    .long BtlSeqReady_Update
    .long BtlSeqReady_Exit
    .space 8
    .long BtlSeqFight_Enter
    .long BtlSeqFight_PreUpdate
    .long BtlSeqFight_Update
    .long BtlSeqFight_Exit
    .space 4
    .long BtlPause_CheckOpen
    .long BtlSeqFinish_Enter
    .long BtlSeqFinish_PreUpdate
    .long BtlSeqFinish_Update
    .long BtlSeqFinish_Exit
    .space 32
    .long BtlSeqEnd_Enter
    .long BtlSeqEnd_PreUpdate
    .long BtlSeqEnd_Update
    .long BtlSeqEnd_Exit
    .space 8
.globl gHudPromptIconDefs
gHudPromptIconDefs:
    .space 192
.globl gHudPromptIconDefs16
gHudPromptIconDefs16:
    .space 204
.globl D_002C63F4
D_002C63F4:
    .space 4
.globl gHudPromptCueDefs
gHudPromptCueDefs:
    .space 48
.globl D_002C6428
D_002C6428:
    .space 8
.globl D_002C6430
D_002C6430:
    .space 8
.globl D_002C6438
D_002C6438:
    .space 8
.globl D_002C6440
D_002C6440:
    .space 8
.globl D_002C6448
D_002C6448:
    .space 8
.globl gFontTags
gFontTags:
    .long D_002C6438
    .space 4
    .long FontTag_PadSequence
    .space 4
    .long D_002C6430
    .space 4
    .long FontTag_Pad
    .space 4
    .long D_002C6440
    .space 4
    .long FontTag_Color
    .space 4
    .long D_002C6448
    .space 4
    .long FontTag_Ub0
    .space 4
.globl D_002C6490
D_002C6490:
    .space 12
.globl D_002C649C
D_002C649C:
    .space 12
.globl D_002C64A8
D_002C64A8:
    .space 12
.globl D_002C64B4
D_002C64B4:
    .space 12
.globl D_002C64C0
D_002C64C0:
    .space 12
.globl D_002C64CC
D_002C64CC:
    .space 12
.globl D_002C64D8
D_002C64D8:
    .space 12
.globl D_002C64E4
D_002C64E4:
    .space 12
.globl D_002C64F0
D_002C64F0:
    .space 12
.globl D_002C64FC
D_002C64FC:
    .space 24
.globl D_002C6514
D_002C6514:
    .space 12
.globl D_002C6520
D_002C6520:
    .space 12
.globl D_002C652C
D_002C652C:
    .space 12
.globl D_002C6538
D_002C6538:
    .space 12
.globl D_002C6544
D_002C6544:
    .space 12
.globl D_002C6550
D_002C6550:
    .space 12
.globl D_002C655C
D_002C655C:
    .space 12
.globl D_002C6568
D_002C6568:
    .space 12
.globl D_002C6574
D_002C6574:
    .space 12
.globl D_002C6580
D_002C6580:
    .space 12
.globl D_002C658C
D_002C658C:
    .space 12
.globl D_002C6598
D_002C6598:
    .space 12
.globl D_002C65A4
D_002C65A4:
    .space 12
.globl D_002C65B0
D_002C65B0:
    .space 12
.globl D_002C65BC
D_002C65BC:
    .space 12
.globl D_002C65C8
D_002C65C8:
    .space 12
.globl D_002C65D4
D_002C65D4:
    .space 12
.globl D_002C65E0
D_002C65E0:
    .space 600
.globl D_002C6838
D_002C6838:
    .space 12
.globl D_002C6844
D_002C6844:
    .space 12
.globl D_002C6850
D_002C6850:
    .space 12
.globl D_002C685C
D_002C685C:
    .space 12
.globl D_002C6868
D_002C6868:
    .space 12
.globl D_002C6874
D_002C6874:
    .space 12
.globl D_002C6880
D_002C6880:
    .space 12
.globl D_002C688C
D_002C688C:
    .space 12
.globl D_002C6898
D_002C6898:
    .space 12
.globl D_002C68A4
D_002C68A4:
    .space 12
.globl D_002C68B0
D_002C68B0:
    .space 12
.globl D_002C68BC
D_002C68BC:
    .space 12
.globl D_002C68C8
D_002C68C8:
    .space 12
.globl D_002C68D4
D_002C68D4:
    .space 12
.globl D_002C68E0
D_002C68E0:
    .space 12
.globl D_002C68EC
D_002C68EC:
    .space 12
.globl D_002C68F8
D_002C68F8:
    .space 12
.globl D_002C6904
D_002C6904:
    .space 12
.globl D_002C6910
D_002C6910:
    .space 12
.globl D_002C691C
D_002C691C:
    .space 12
.globl D_002C6928
D_002C6928:
    .space 12
.globl D_002C6934
D_002C6934:
    .space 12
.globl D_002C6940
D_002C6940:
    .space 12
.globl D_002C694C
D_002C694C:
    .space 12
.globl D_002C6958
D_002C6958:
    .space 12
.globl D_002C6964
D_002C6964:
    .space 12
.globl D_002C6970
D_002C6970:
    .space 12
.globl D_002C697C
D_002C697C:
    .space 12
.globl D_002C6988
D_002C6988:
    .long D_002C6490
    .long D_002C6838
    .space 4
    .long D_002C649C
    .long D_002C6844
    .space 4
    .long D_002C64A8
    .long D_002C6850
    .space 4
    .long D_002C64B4
    .long D_002C685C
    .space 4
    .long D_002C64C0
    .long D_002C6868
    .space 4
    .long D_002C64CC
    .long D_002C6874
    .space 4
    .long D_002C64D8
    .long D_002C6880
    .space 4
    .long D_002C64E4
    .long D_002C688C
    .space 4
    .long D_002C64F0
    .long D_002C6898
    .space 4
    .long D_002C64FC
    .long D_002C68A4
    .space 4
    .long D_002C6514
    .long D_002C68B0
    .space 4
    .long D_002C6520
    .long D_002C68BC
    .space 4
    .long D_002C652C
    .long D_002C68C8
    .space 4
    .long D_002C6538
    .long D_002C68D4
    .space 4
    .long D_002C6544
    .long D_002C68E0
    .space 4
    .long D_002C6550
    .long D_002C68EC
    .space 4
    .long D_002C655C
    .long D_002C68F8
    .space 4
    .long D_002C6568
    .long D_002C6904
    .space 4
    .long D_002C6574
    .long D_002C6910
    .space 4
    .long D_002C6580
    .long D_002C691C
    .space 4
    .long D_002C658C
    .long D_002C6928
    .space 4
    .long D_002C6598
    .long D_002C6934
    .space 4
    .long D_002C65A4
    .long D_002C6940
    .space 4
    .long D_002C65B0
    .long D_002C694C
    .space 4
    .long D_002C65BC
    .long D_002C6958
    .space 4
    .long D_002C65C8
    .long D_002C6964
    .space 4
    .long D_002C65D4
    .long D_002C6970
    .space 4
    .long D_002C65E0
    .long D_002C697C
    .space 20
.globl D_002C6AE8
D_002C6AE8:
    .space 16
.globl D_002C6AF8
D_002C6AF8:
    .space 16
.globl D_002C6B08
D_002C6B08:
    .space 16
.globl gFontIconTables
gFontIconTables:
    .long D_002C6988
    .long D_002C6AE8
    .long D_002C6AF8
    .long D_002C6B08
.globl D_002C6B28
D_002C6B28:
    .space 8
.globl gStgDbgPlaces
gStgDbgPlaces:
    .space 192
.globl gStgLastLightVecA
gStgLastLightVecA:
    .space 16
.globl gStgLastLightVecB
gStgLastLightVecB:
    .space 16
.globl D_002C6C10
D_002C6C10:
    .space 16
.globl D_002C6C20
D_002C6C20:
    .space 32
.globl D_002C6C40
D_002C6C40:
    .space 24
.globl D_002C6C58
D_002C6C58:
    .space 64
.globl D_002C6C98
D_002C6C98:
    .space 64
.globl D_002C6CD8
D_002C6CD8:
    .space 64
.globl D_002C6D18
D_002C6D18:
    .space 64
.globl D_002C6D58
D_002C6D58:
    .space 64
.globl D_002C6D98
D_002C6D98:
    .space 64
.globl D_002C6DD8
D_002C6DD8:
    .space 64
.globl D_002C6E18
D_002C6E18:
    .space 72
.globl gFadeColors
gFadeColors:
    .space 96
.globl gBtlScriptCommands
gBtlScriptCommands:
    .space 4
    .long BtlScriptCmd_Wait
    .space 4
    .long BtlScriptCmd_SetStory
    .space 4
    .long BtlScriptCmd_Nop3
    .space 4
    .long BtlScriptCmd_EndEvent
    .space 4
    .long BtlScriptCmd_BeginScene
    .space 4
    .long BtlScriptCmd_EndScene
    .space 4
    .long BtlScriptCmd_Battle
    .space 4
    .long BtlScriptCmd_SetResult
    .space 4
    .long BtlScriptCmd_SetTriggers
    .space 4
    .long BtlScriptCmd_SetRule
    .space 4
    .long BtlScriptCmd_SetSide
    .space 4
    .long BtlScriptCmd_SetMember
    .space 4
    .long BtlScriptCmd_SetStatus0
    .space 4
    .long BtlScriptCmd_SetStatus1
    .space 4
    .long BtlScriptCmd_SetSpeakers
    .space 4
    .long BtlScriptCmd_SetRewards
    .space 4
    .long BtlScriptCmd_PlaceChar
    .space 4
    .long BtlScriptCmd_SetCharPos
    .space 4
    .long BtlScriptCmd_SetCharDir
    .space 4
    .long BtlScriptCmd_ClearCtrl100
    .space 4
    .long BtlScriptCmd_SetCtrl100
    .space 4
    .long BtlScriptCmd_SetCtrlFE
    .space 4
    .long BtlScriptCmd_SetCtrlFF
    .space 4
    .long BtlScriptCmd_SetCtrl101
    .space 4
    .long BtlScriptCmd_ClearCtrl101
    .space 4
    .long BtlScriptCmd_SetCtrl102
    .space 4
    .long BtlScriptCmd_MoveChar
    .space 4
    .long BtlScriptCmd_StopCharMoves
    .space 4
    .long BtlScriptCmd_LoadText
    .space 4
    .long BtlScriptCmd_LoadFile8
    .space 4
    .long BtlScriptCmd_Nop1301
    .space 4
    .long BtlScriptCmd_SetWindow
    .space 4
    .long BtlScriptCmd_FadeIn
    .space 4
    .long BtlScriptCmd_FadeOut
    .space 4
    .long BtlScriptCmd_ResetFades
    .space 4
    .long BtlScriptCmd_SetCamera
    .space 4
    .long BtlScriptCmd_MoveCamera
    .space 4
    .long BtlScriptCmd_ClearCamera
    .space 4
    .long BtlScriptCmd_ShakeCamera
    .space 4
    .long BtlScriptCmd_StopShake
    .space 4
    .long BtlScriptCmd_PlayBgm
    .space 4
    .long BtlScriptCmd_StopBgm
    .space 4
    .long BtlScriptCmd_PlaySe
    .space 4
    .long BtlScriptCmd_PlayVoice
    .space 4
    .long BtlScriptCmd_Talk
    .space 4
    .long BtlScriptCmd_WaitButton
    .space 8
.globl gAfsPartitionTbl
gAfsPartitionTbl:
    .long D_002F34B0
    .space 8
    .long D_002F34A0
    .space 8
    .long D_002F3490
    .space 8
.globl D_002C705C
D_002C705C:
    .space 4
.globl gAdxPlayerTbl
gAdxPlayerTbl:
    .long D_002FF148
    .space 16
    .long D_002FF140
    .space 16
    .long D_002FF138
    .space 16
    .long D_002FF138
    .space 16
    .long D_002FF130
    .space 16
    .long D_002FF130
    .space 16
.globl gDiscExeNameTbl
gDiscExeNameTbl:
    .long D_002F34F0
    .long D_002F34E0
    .long D_002F34D0
.globl D_002C70E4
D_002C70E4:
    .space 4
.globl gAdxChannelGainTbl
gAdxChannelGainTbl:
    .space 24
.globl D_002C7100
D_002C7100:
    .space 4
.globl D_002C7104
D_002C7104:
    .space 147
.globl D_002C7197
D_002C7197:
    .space 8401
.globl D_002C9268
D_002C9268:
    .space 8
.globl D_002C9270
D_002C9270:
    .space 4
.globl D_002C9274
D_002C9274:
    .space 4
.globl D_002C9278
D_002C9278:
    .space 4
.globl D_002C927C
D_002C927C:
    .space 4
.globl D_002C9280
D_002C9280:
    .space 8
.globl D_002C9288
D_002C9288:
    .space 3208
.globl D_002C9F10
D_002C9F10:
    .long D_002F40E0
.globl D_002C9F14
D_002C9F14:
    .space 4
.globl D_002C9F18
D_002C9F18:
    .space 8
.globl D_002C9F20
D_002C9F20:
    .space 4
.globl D_002C9F24
D_002C9F24:
    .space 4
.globl D_002C9F28
D_002C9F28:
    .space 4
.globl D_002C9F2C
D_002C9F2C:
    .space 4
.globl D_002C9F30
D_002C9F30:
    .space 8
.globl D_002C9F38
D_002C9F38:
    .space 4
.globl D_002C9F3C
D_002C9F3C:
    .space 20
.globl D_002C9F50
D_002C9F50:
    .space 4
.globl D_002C9F54
D_002C9F54:
    .space 4
.globl D_002C9F58
D_002C9F58:
    .space 4
.globl D_002C9F5C
D_002C9F5C:
    .space 4
.globl D_002C9F60
D_002C9F60:
    .space 4
.globl D_002C9F64
D_002C9F64:
    .space 4
.globl D_002C9F68
D_002C9F68:
    .space 4
.globl D_002C9F6C
D_002C9F6C:
    .space 4
.globl D_002C9F70
D_002C9F70:
    .space 8
.globl D_002C9F78
D_002C9F78:
    .space 8
.globl D_002C9F80
D_002C9F80:
    .space 8
.globl D_002C9F88
D_002C9F88:
    .space 8
.globl D_002C9F90
D_002C9F90:
    .space 8
.globl D_002C9F98
D_002C9F98:
    .space 8
.globl D_002C9FA0
D_002C9FA0:
    .space 8
.globl D_002C9FA8
D_002C9FA8:
    .space 4
.globl D_002C9FAC
D_002C9FAC:
    .space 4
.globl D_002C9FB0
D_002C9FB0:
    .space 4
.globl D_002C9FB4
D_002C9FB4:
    .space 4
.globl D_002C9FB8
D_002C9FB8:
    .space 4
.globl D_002C9FBC
D_002C9FBC:
    .space 4
.globl D_002C9FC0
D_002C9FC0:
    .space 8
.globl D_002C9FC8
D_002C9FC8:
    .space 8
.globl D_002C9FD0
D_002C9FD0:
    .space 8
.globl D_002C9FD8
D_002C9FD8:
    .space 8
.globl D_002C9FE0
D_002C9FE0:
    .space 8
.globl D_002C9FE8
D_002C9FE8:
    .space 8
.globl D_002C9FF0
D_002C9FF0:
    .space 8
.globl D_002C9FF8
D_002C9FF8:
    .space 8
.globl D_002CA000
D_002CA000:
    .space 8
.globl D_002CA008
D_002CA008:
    .space 8
.globl D_002CA010
D_002CA010:
    .space 8
.globl D_002CA018
D_002CA018:
    .space 8
.globl D_002CA020
D_002CA020:
    .space 8
.globl D_002CA028
D_002CA028:
    .space 4
.globl D_002CA02C
D_002CA02C:
    .space 4
.globl D_002CA030
D_002CA030:
    .space 4
.globl D_002CA034
D_002CA034:
    .space 4
.globl D_002CA038
D_002CA038:
    .space 4
.globl D_002CA03C
D_002CA03C:
    .space 4
.globl D_002CA040
D_002CA040:
    .space 2048
.globl D_002CA840
D_002CA840:
    .space 4096
.globl D_002CB840
D_002CB840:
    .space 4096
.globl D_002CC840
D_002CC840:
    .space 4096
.globl D_002CD840
D_002CD840:
    .space 8192
.globl D_002CF840
D_002CF840:
    .space 2224
.globl D_002D00F0
D_002D00F0:
    .space 5968
.globl D_002D1840
D_002D1840:
    .space 4
.globl D_002D1844
D_002D1844:
    .space 4
.globl D_002D1848
D_002D1848:
    .space 4
.globl D_002D184C
D_002D184C:
    .space 4
.globl D_002D1850
D_002D1850:
    .space 8
.globl D_002D1858
D_002D1858:
    .space 4
.globl D_002D185C
D_002D185C:
    .space 4
.globl D_002D1860
D_002D1860:
    .space 2752
.globl D_002D2320
D_002D2320:
    .space 4
.globl D_002D2324
D_002D2324:
    .space 4
.globl D_002D2328
D_002D2328:
    .space 4
.globl D_002D232C
D_002D232C:
    .space 4
.globl D_002D2330
D_002D2330:
    .space 4
.globl D_002D2334
D_002D2334:
    .space 4
.globl D_002D2338
D_002D2338:
    .space 4
.globl D_002D233C
D_002D233C:
    .space 4
.globl D_002D2340
D_002D2340:
    .space 3840
.globl D_002D3240
D_002D3240:
    .space 16
.globl D_002D3250
D_002D3250:
    .space 32
.globl D_002D3270
D_002D3270:
    .space 4
.globl D_002D3274
D_002D3274:
    .space 4
.globl D_002D3278
D_002D3278:
    .space 4
.globl D_002D327C
D_002D327C:
    .space 4
.globl D_002D3280
D_002D3280:
    .space 4
.globl D_002D3284
D_002D3284:
    .space 4
.globl D_002D3288
D_002D3288:
    .space 4
.globl D_002D328C
D_002D328C:
    .space 4
.globl D_002D3290
D_002D3290:
    .space 4
.globl D_002D3294
D_002D3294:
    .space 4
.globl D_002D3298
D_002D3298:
    .space 4
.globl D_002D329C
D_002D329C:
    .space 40
.globl D_002D32C4
D_002D32C4:
    .space 4
.globl D_002D32C8
D_002D32C8:
    .space 4
.globl D_002D32CC
D_002D32CC:
    .space 4
.globl D_002D32D0
D_002D32D0:
    .space 4
.globl D_002D32D4
D_002D32D4:
    .space 4
.globl D_002D32D8
D_002D32D8:
    .space 60
.globl D_002D3314
D_002D3314:
    .space 4
.globl D_002D3318
D_002D3318:
    .space 4
.globl D_002D331C
D_002D331C:
    .space 4
.globl D_002D3320
D_002D3320:
    .space 1
.globl D_002D3321
D_002D3321:
    .space 1
.globl D_002D3322
D_002D3322:
    .space 50
.globl D_002D3354
D_002D3354:
    .space 4
.globl D_002D3358
D_002D3358:
    .long D_002F59B0
.globl D_002D335C
D_002D335C:
    .space 4
.globl D_002D3360
D_002D3360:
    .space 4
.globl D_002D3364
D_002D3364:
    .space 4
.globl D_002D3368
D_002D3368:
    .space 4
.globl D_002D336C
D_002D336C:
    .space 4
.globl D_002D3370
D_002D3370:
    .space 4
.globl D_002D3374
D_002D3374:
    .space 4
.globl D_002D3378
D_002D3378:
    .space 4
.globl D_002D337C
D_002D337C:
    .space 4
.globl D_002D3380
D_002D3380:
    .space 4
.globl D_002D3384
D_002D3384:
    .space 4
.globl D_002D3388
D_002D3388:
    .space 4
.globl D_002D338C
D_002D338C:
    .space 4
.globl D_002D3390
D_002D3390:
    .space 4
.globl D_002D3394
D_002D3394:
    .space 4
.globl D_002D3398
D_002D3398:
    .space 4
.globl D_002D339C
D_002D339C:
    .space 4
.globl D_002D33A0
D_002D33A0:
    .space 4
.globl D_002D33A4
D_002D33A4:
    .space 4
.globl D_002D33A8
D_002D33A8:
    .space 4
.globl D_002D33AC
D_002D33AC:
    .space 8
.globl D_002D33B4
D_002D33B4:
    .space 4
.globl D_002D33B8
D_002D33B8:
    .space 4
.globl D_002D33BC
D_002D33BC:
    .space 4
.globl D_002D33C0
D_002D33C0:
    .space 4
.globl D_002D33C4
D_002D33C4:
    .space 4
.globl D_002D33C8
D_002D33C8:
    .space 4
.globl D_002D33CC
D_002D33CC:
    .space 4
.globl D_002D33D0
D_002D33D0:
    .space 2176
.globl D_002D3C50
D_002D3C50:
    .space 40
.globl D_002D3C78
D_002D3C78:
    .space 72
.globl D_002D3CC0
D_002D3CC0:
    .space 32
.globl D_002D3CE0
D_002D3CE0:
    .space 256
.globl D_002D3DE0
D_002D3DE0:
    .space 256
.globl D_002D3EE0
D_002D3EE0:
    .space 256
.globl D_002D3FE0
D_002D3FE0:
    .long D_002F5E40
.globl D_002D3FE4
D_002D3FE4:
    .space 4
.globl D_002D3FE8
D_002D3FE8:
    .space 4
.globl D_002D3FEC
D_002D3FEC:
    .space 4
.globl D_002D3FF0
D_002D3FF0:
    .space 8
.globl D_002D3FF8
D_002D3FF8:
    .long func_0027BCD0
    .long func_0027BD48
    .long dvCiGetFileSize
    .space 4
    .long dvCiOpen
    .long func_0027C0F0
    .long func_0027C148
    .long func_0027C1E0
    .long dvCiReqRd
    .space 4
    .long func_0027C3C0
    .long func_0027C4A8
    .long func_0027C4E0
    .space 4
    .long func_0027C4E8
    .space 4
    .long func_0027BDD8
    .space 28
    .long func_0027C680
    .space 4
.globl D_002D4060
D_002D4060:
    .space 2880
.globl D_002D4BA0
D_002D4BA0:
    .space 304
.globl D_002D4CD0
D_002D4CD0:
    .space 16
.globl D_002D4CE0
D_002D4CE0:
    .space 4
.globl D_002D4CE4
D_002D4CE4:
    .space 4
.globl D_002D4CE8
D_002D4CE8:
    .space 8
.globl D_002D4CF0
D_002D4CF0:
    .space 264
.globl D_002D4DF8
D_002D4DF8:
    .space 128
.globl D_002D4E78
D_002D4E78:
    .space 16
.globl D_002D4E88
D_002D4E88:
    .space 4
.globl D_002D4E8C
D_002D4E8C:
    .space 4
.globl D_002D4E90
D_002D4E90:
    .space 264
.globl D_002D4F98
D_002D4F98:
    .space 36
.globl D_002D4FBC
D_002D4FBC:
    .space 4
.globl D_002D4FC0
D_002D4FC0:
    .space 4
.globl D_002D4FC4
D_002D4FC4:
    .space 4
.globl D_002D4FC8
D_002D4FC8:
    .space 4
.globl D_002D4FCC
D_002D4FCC:
    .space 4
.globl D_002D4FD0
D_002D4FD0:
    .space 256
.globl D_002D50D0
D_002D50D0:
    .long D_002F6820
    .space 20
.globl D_002D50E8
D_002D50E8:
    .space 8
.globl D_002D50F0
D_002D50F0:
    .space 18176
.globl D_002D97F0
D_002D97F0:
    .long D_002F68A8
.globl D_002D97F4
D_002D97F4:
    .space 4
.globl D_002D97F8
D_002D97F8:
    .space 8
.globl D_002D9800
D_002D9800:
    .long func_0027F7A0
    .long func_0027F7F8
    .long func_0027F810
    .space 4
    .long mfCiOpen
    .long func_0027F9B8
    .long func_0027FA00
    .long func_0027FAC0
    .long mfCiReqRd
    .space 4
    .long func_0027FC80
    .long func_0027FCD0
    .long func_0027FD08
    .long func_0027FD40
    .long func_0027FDC0
    .space 36
    .long func_0027FE80
    .space 4
.globl D_002D9868
D_002D9868:
    .space 2544
.globl D_002DA258
D_002DA258:
    .long D_002F6BB8
    .space 4
.globl D_002DA260
D_002DA260:
    .space 4000
.globl D_002DB200
D_002DB200:
    .space 4
.globl D_002DB204
D_002DB204:
    .space 4
.globl D_002DB208
D_002DB208:
    .space 4
.globl D_002DB20C
D_002DB20C:
    .space 4
.globl D_002DB210
D_002DB210:
    .space 4
.globl D_002DB214
D_002DB214:
    .space 4
.globl D_002DB218
D_002DB218:
    .space 4
.globl D_002DB21C
D_002DB21C:
    .space 4
.globl D_002DB220
D_002DB220:
    .space 4
.globl D_002DB224
D_002DB224:
    .space 4
.globl D_002DB228
D_002DB228:
    .space 4
.globl D_002DB22C
D_002DB22C:
    .space 4
.globl D_002DB230
D_002DB230:
    .space 4
.globl D_002DB234
D_002DB234:
    .space 4
.globl D_002DB238
D_002DB238:
    .space 8
.globl D_002DB240
D_002DB240:
    .space 8
.globl D_002DB248
D_002DB248:
    .space 4
.globl D_002DB24C
D_002DB24C:
    .space 4
.globl D_002DB250
D_002DB250:
    .space 4
.globl D_002DB254
D_002DB254:
    .space 4
.globl D_002DB258
D_002DB258:
    .space 4
.globl D_002DB25C
D_002DB25C:
    .space 2
.globl D_002DB25E
D_002DB25E:
    .space 2
.globl D_002DB260
D_002DB260:
    .space 768
.globl D_002DB560
D_002DB560:
    .space 8448
.globl D_002DD660
D_002DD660:
    .space 1600
.globl D_002DDCA0
D_002DDCA0:
    .space 4
.globl D_002DDCA4
D_002DDCA4:
    .space 4
.globl D_002DDCA8
D_002DDCA8:
    .space 2256
.globl D_002DE578
D_002DE578:
    .space 8
.globl D_002DE580
D_002DE580:
    .space 8
.globl D_002DE588
D_002DE588:
    .space 8
.globl D_002DE590
D_002DE590:
    .space 12
    .long func_002826A0
    .long func_00282758
    .long func_00282898
    .long func_00282978
    .long func_00282BA8
    .long func_00282AB8
    .long func_00282930
    .long func_00282CF8
    .long func_002827E8
.globl D_002DE5C0
D_002DE5C0:
    .space 8
.globl D_002DE5C8
D_002DE5C8:
    .space 1200
.globl D_002DEA78
D_002DEA78:
    .space 12
    .long func_00283278
    .long func_00283330
    .long func_00283348
    .long func_00283448
    .long func_00283818
    .long func_00283648
    .long func_002833F8
    .long func_00283A48
    .long func_00283338
.globl D_002DEAA8
D_002DEAA8:
    .space 8
.globl D_002DEAB0
D_002DEAB0:
    .space 16384
.globl D_002E2AB0
D_002E2AB0:
    .space 12
    .long func_00283E98
    .long func_00283F50
    .long func_00284090
    .long func_00284298
    .long func_002845C0
    .long func_00284448
    .long func_00284198
    .long func_00284730
    .long func_00283FE0
.globl D_002E2AE0
D_002E2AE0:
    .space 8
.globl D_002E2AE8
D_002E2AE8:
    .space 3072
.globl D_002E36E8
D_002E36E8:
    .space 24
.globl D_002E3700
D_002E3700:
    .space 448
.globl D_002E38C0
D_002E38C0:
    .long D_002F7608
.globl D_002E38C4
D_002E38C4:
    .space 4
    .long D_002F7638
.globl D_002E38CC
D_002E38CC:
    .space 4
.globl D_002E38D0
D_002E38D0:
    .space 4
.globl D_002E38D4
D_002E38D4:
    .space 4
.globl D_002E38D8
D_002E38D8:
    .space 8
.globl D_002E38E0
D_002E38E0:
    .space 640
.globl D_002E3B60
D_002E3B60:
    .space 4
.globl D_002E3B64
D_002E3B64:
    .space 4
.globl D_002E3B68
D_002E3B68:
    .space 2256
    .long D_002F76E8
.globl D_002E443C
D_002E443C:
    .space 4
.globl D_002E4440
D_002E4440:
    .space 4
.globl D_002E4444
D_002E4444:
    .space 4
.globl D_002E4448
D_002E4448:
    .space 32
.globl D_002E4468
D_002E4468:
    .space 32
.globl D_002E4488
D_002E4488:
    .space 128
.globl D_002E4508
D_002E4508:
    .space 8
.globl D_002E4510
D_002E4510:
    .space 768
.globl D_002E4810
D_002E4810:
    .space 4
.globl D_002E4814
D_002E4814:
    .space 4
.globl D_002E4818
D_002E4818:
    .space 4
.globl D_002E481C
D_002E481C:
    .space 4
.globl D_002E4820
D_002E4820:
    .space 4
.globl D_002E4824
D_002E4824:
    .space 4
.globl D_002E4828
D_002E4828:
    .space 8
    .long D_002F78F0
.globl D_002E4834
D_002E4834:
    .space 4
.globl D_002E4838
D_002E4838:
    .space 4
.globl D_002E483C
D_002E483C:
    .space 4
.globl D_002E4840
D_002E4840:
    .space 4
.globl D_002E4844
D_002E4844:
    .space 4
.globl D_002E4848
D_002E4848:
    .space 4
.globl D_002E484C
D_002E484C:
    .space 2
.globl D_002E484E
D_002E484E:
    .space 2
.globl D_002E4850
D_002E4850:
    .space 4
.globl D_002E4854
D_002E4854:
    .space 4
.globl D_002E4858
D_002E4858:
    .space 4160
.globl D_002E5898
D_002E5898:
    .long D_002F81A8
.globl D_002E589C
D_002E589C:
    .long D_002F81B0
.globl D_002E58A0
D_002E58A0:
    .space 8
.globl D_002E58A8
D_002E58A8:
    .space 8
.globl D_002E58B0
D_002E58B0:
    .space 4
.globl D_002E58B4
D_002E58B4:
    .space 4
.globl D_002E58B8
D_002E58B8:
    .space 256
.globl D_002E59B8
D_002E59B8:
    .space 8
.globl D_002E59C0
D_002E59C0:
    .space 1152
.globl D_002E5E40
D_002E5E40:
    .space 1024
.globl D_002E6240
D_002E6240:
    .space 8
.globl D_002E6248
D_002E6248:
    .space 256
.globl D_002E6348
D_002E6348:
    .space 32
.globl D_002E6368
D_002E6368:
    .space 4
.globl D_002E636C
D_002E636C:
    .space 4
.globl D_002E6370
D_002E6370:
    .space 4
.globl D_002E6374
D_002E6374:
    .space 4
.globl D_002E6378
D_002E6378:
    .space 4
.globl D_002E637C
D_002E637C:
    .space 4
.globl D_002E6380
D_002E6380:
    .space 8
.globl D_002E6388
D_002E6388:
    .space 8
.globl D_002E6390
D_002E6390:
    .space 960
    .long D_002F8280
.globl D_002E6754
D_002E6754:
    .space 4
.globl D_002E6758
D_002E6758:
    .space 2048
.globl D_002E6F58
D_002E6F58:
    .long D_002F83C0
.globl D_002E6F5C
D_002E6F5C:
    .space 4
.globl D_002E6F60
D_002E6F60:
    .space 4
.globl D_002E6F64
D_002E6F64:
    .space 4
.globl D_002E6F68
D_002E6F68:
    .long func_0028B7C0
    .long func_0028B838
    .long func_0028B8F0
    .space 4
    .long htCiOpen
    .long func_0028BD28
    .long func_0028BDC8
    .long func_0028BE60
    .long htCiReqRd
    .space 4
    .long func_0028C028
    .long func_0028C120
    .long func_0028C158
    .space 4
    .long func_0028C160
    .space 4
    .long func_0028B850
    .space 28
    .long func_0028C288
    .space 4
.globl D_002E6FD0
D_002E6FD0:
    .space 624
.globl D_002E7240
D_002E7240:
    .space 304
.globl D_002E7370
D_002E7370:
    .space 48
.globl D_002E73A0
D_002E73A0:
    .space 4
.globl D_002E73A4
D_002E73A4:
    .space 4
.globl D_002E73A8
D_002E73A8:
    .space 8
.globl D_002E73B0
D_002E73B0:
    .space 512
.globl D_002E75B0
D_002E75B0:
    .space 32
.globl D_002E75D0
D_002E75D0:
    .space 16
.globl D_002E75E0
D_002E75E0:
    .space 32
.globl D_002E7600
D_002E7600:
    .space 32
.globl D_002E7620
D_002E7620:
    .space 4
.globl D_002E7624
D_002E7624:
    .space 20
.globl D_002E7638
D_002E7638:
    .space 16
.globl D_002E7648
D_002E7648:
    .long D_002FA3E0
    .long D_002FA3D8
    .long D_002FA3D0
    .long D_002FA3C8
.globl D_002E7658
D_002E7658:
    .space 4
.globl D_002E765C
D_002E765C:
    .space 20
.globl D_002E7670
D_002E7670:
    .space 4
.globl D_002E7674
D_002E7674:
    .space 4
.globl D_002E7678
D_002E7678:
    .space 4
.globl D_002E767C
D_002E767C:
    .space 4
.globl D_002E7680
D_002E7680:
    .space 4
.globl D_002E7684
D_002E7684:
    .space 4
.globl D_002E7688
D_002E7688:
    .space 4
.globl D_002E768C
D_002E768C:
    .space 4
.globl D_002E7690
D_002E7690:
    .space 4
.globl D_002E7694
D_002E7694:
    .space 20
.globl D_002E76A8
D_002E76A8:
    .space 64
.globl D_002E76E8
D_002E76E8:
    .space 160
.globl D_002E7788
D_002E7788:
    .long func_0029F2F8
    .long func_0029F408
    .long func_0029F590
    .long func_0029F6F8
    .long func_0029F8F0
    .long func_0029FA40
    .long func_0029FC08
    .long func_0029FDB0
.globl D_002E77A8
D_002E77A8:
    .long func_0029F370
    .long func_0029F4C0
    .long func_0029F640
    .long func_0029F7F0
    .long func_0029F990
    .long func_0029FB20
    .long func_0029FCE0
    .long func_0029FED0
    .space 56
.globl D_002E7800
D_002E7800:
    .space 64
.globl D_002E7840
D_002E7840:
    .space 80
.globl D_002E7890
D_002E7890:
    .space 80
.globl D_002E78E0
D_002E78E0:
    .space 48
.globl D_002E7910
D_002E7910:
    .space 4
.globl D_002E7914
D_002E7914:
    .space 4
.globl D_002E7918
D_002E7918:
    .space 56
.globl D_002E7950
D_002E7950:
    .space 4
.globl D_002E7954
D_002E7954:
    .space 4
.globl D_002E7958
D_002E7958:
    .space 4
.globl D_002E795C
D_002E795C:
    .space 4
.globl D_002E7960
D_002E7960:
    .space 4
.globl D_002E7964
D_002E7964:
    .space 4
.globl D_002E7968
D_002E7968:
    .space 4
.globl D_002E796C
D_002E796C:
    .space 4
.globl D_002E7970
D_002E7970:
    .space 4
.globl D_002E7974
D_002E7974:
    .space 4
.globl D_002E7978
D_002E7978:
    .space 4
.globl D_002E797C
D_002E797C:
    .space 4
.globl D_002E7980
D_002E7980:
    .space 4
.globl D_002E7984
D_002E7984:
    .space 4
.globl D_002E7988
D_002E7988:
    .space 4
.globl D_002E798C
D_002E798C:
    .space 4
.globl D_002E7990
D_002E7990:
    .space 4
.globl D_002E7994
D_002E7994:
    .space 4
.globl D_002E7998
D_002E7998:
    .space 4
.globl D_002E799C
D_002E799C:
    .space 4
.globl D_002E79A0
D_002E79A0:
    .space 4
.globl D_002E79A4
D_002E79A4:
    .space 28
.globl D_002E79C0
D_002E79C0:
    .space 128
.globl D_002E7A40
D_002E7A40:
    .space 4096
.globl D_002E8A40
D_002E8A40:
    .space 192
.globl D_002E8B00
D_002E8B00:
    .space 16
.globl D_002E8B10
D_002E8B10:
    .space 48
.globl D_002E8B40
D_002E8B40:
    .space 1088
.globl D_002E8F80
D_002E8F80:
    .space 1088
.globl D_002E93C0
D_002E93C0:
    .space 256
.globl D_002E94C0
D_002E94C0:
    .space 40
.globl D_002E94E8
D_002E94E8:
    .space 4
.globl D_002E94EC
D_002E94EC:
    .space 4
.globl D_002E94F0
D_002E94F0:
    .space 24
.globl D_002E9508
D_002E9508:
    .space 4
.globl D_002E950C
D_002E950C:
    .space 12
.globl D_002E9518
D_002E9518:
    .space 4
    .long D_002E96FC
    .long D_002E9754
    .long D_002E97AC
    .space 36
    .long D_002FB368
    .space 428
.globl D_002E96FC
D_002E96FC:
    .space 88
.globl D_002E9754
D_002E9754:
    .space 88
.globl D_002E97AC
D_002E97AC:
    .space 92
.globl D_002E9808
D_002E9808:
    .long D_002E9518
    .space 4
.globl D_002E9810
D_002E9810:
    .space 8
.globl D_002E9818
D_002E9818:
    .long D_002E9810
    .long D_002E9810
.globl D_002E9820
D_002E9820:
    .long D_002E9818
    .long D_002E9818
.globl D_002E9828
D_002E9828:
    .long D_002E9820
    .long D_002E9820
.globl D_002E9830
D_002E9830:
    .long D_002E9828
    .long D_002E9828
.globl D_002E9838
D_002E9838:
    .long D_002E9830
    .long D_002E9830
.globl D_002E9840
D_002E9840:
    .long D_002E9838
    .long D_002E9838
.globl D_002E9848
D_002E9848:
    .long D_002E9840
    .long D_002E9840
.globl D_002E9850
D_002E9850:
    .long D_002E9848
    .long D_002E9848
.globl D_002E9858
D_002E9858:
    .long D_002E9850
    .long D_002E9850
.globl D_002E9860
D_002E9860:
    .long D_002E9858
    .long D_002E9858
.globl D_002E9868
D_002E9868:
    .long D_002E9860
    .long D_002E9860
.globl D_002E9870
D_002E9870:
    .long D_002E9868
    .long D_002E9868
.globl D_002E9878
D_002E9878:
    .long D_002E9870
    .long D_002E9870
.globl D_002E9880
D_002E9880:
    .long D_002E9878
    .long D_002E9878
.globl D_002E9888
D_002E9888:
    .long D_002E9880
    .long D_002E9880
.globl D_002E9890
D_002E9890:
    .long D_002E9888
    .long D_002E9888
.globl D_002E9898
D_002E9898:
    .long D_002E9890
    .long D_002E9890
.globl D_002E98A0
D_002E98A0:
    .long D_002E9898
    .long D_002E9898
.globl D_002E98A8
D_002E98A8:
    .long D_002E98A0
    .long D_002E98A0
.globl D_002E98B0
D_002E98B0:
    .long D_002E98A8
    .long D_002E98A8
.globl D_002E98B8
D_002E98B8:
    .long D_002E98B0
    .long D_002E98B0
.globl D_002E98C0
D_002E98C0:
    .long D_002E98B8
    .long D_002E98B8
.globl D_002E98C8
D_002E98C8:
    .long D_002E98C0
    .long D_002E98C0
.globl D_002E98D0
D_002E98D0:
    .long D_002E98C8
    .long D_002E98C8
.globl D_002E98D8
D_002E98D8:
    .long D_002E98D0
    .long D_002E98D0
.globl D_002E98E0
D_002E98E0:
    .long D_002E98D8
    .long D_002E98D8
.globl D_002E98E8
D_002E98E8:
    .long D_002E98E0
    .long D_002E98E0
.globl D_002E98F0
D_002E98F0:
    .long D_002E98E8
    .long D_002E98E8
.globl D_002E98F8
D_002E98F8:
    .long D_002E98F0
    .long D_002E98F0
.globl D_002E9900
D_002E9900:
    .long D_002E98F8
    .long D_002E98F8
.globl D_002E9908
D_002E9908:
    .long D_002E9900
    .long D_002E9900
.globl D_002E9910
D_002E9910:
    .long D_002E9908
    .long D_002E9908
.globl D_002E9918
D_002E9918:
    .long D_002E9910
    .long D_002E9910
.globl D_002E9920
D_002E9920:
    .long D_002E9918
    .long D_002E9918
.globl D_002E9928
D_002E9928:
    .long D_002E9920
    .long D_002E9920
.globl D_002E9930
D_002E9930:
    .long D_002E9928
    .long D_002E9928
.globl D_002E9938
D_002E9938:
    .long D_002E9930
    .long D_002E9930
.globl D_002E9940
D_002E9940:
    .long D_002E9938
    .long D_002E9938
.globl D_002E9948
D_002E9948:
    .long D_002E9940
    .long D_002E9940
.globl D_002E9950
D_002E9950:
    .long D_002E9948
    .long D_002E9948
.globl D_002E9958
D_002E9958:
    .long D_002E9950
    .long D_002E9950
.globl D_002E9960
D_002E9960:
    .long D_002E9958
    .long D_002E9958
.globl D_002E9968
D_002E9968:
    .long D_002E9960
    .long D_002E9960
.globl D_002E9970
D_002E9970:
    .long D_002E9968
    .long D_002E9968
.globl D_002E9978
D_002E9978:
    .long D_002E9970
    .long D_002E9970
.globl D_002E9980
D_002E9980:
    .long D_002E9978
    .long D_002E9978
.globl D_002E9988
D_002E9988:
    .long D_002E9980
    .long D_002E9980
.globl D_002E9990
D_002E9990:
    .long D_002E9988
    .long D_002E9988
.globl D_002E9998
D_002E9998:
    .long D_002E9990
    .long D_002E9990
.globl D_002E99A0
D_002E99A0:
    .long D_002E9998
    .long D_002E9998
.globl D_002E99A8
D_002E99A8:
    .long D_002E99A0
    .long D_002E99A0
.globl D_002E99B0
D_002E99B0:
    .long D_002E99A8
    .long D_002E99A8
.globl D_002E99B8
D_002E99B8:
    .long D_002E99B0
    .long D_002E99B0
.globl D_002E99C0
D_002E99C0:
    .long D_002E99B8
    .long D_002E99B8
.globl D_002E99C8
D_002E99C8:
    .long D_002E99C0
    .long D_002E99C0
.globl D_002E99D0
D_002E99D0:
    .long D_002E99C8
    .long D_002E99C8
.globl D_002E99D8
D_002E99D8:
    .long D_002E99D0
    .long D_002E99D0
.globl D_002E99E0
D_002E99E0:
    .long D_002E99D8
    .long D_002E99D8
.globl D_002E99E8
D_002E99E8:
    .long D_002E99E0
    .long D_002E99E0
.globl D_002E99F0
D_002E99F0:
    .long D_002E99E8
    .long D_002E99E8
.globl D_002E99F8
D_002E99F8:
    .long D_002E99F0
    .long D_002E99F0
.globl D_002E9A00
D_002E9A00:
    .long D_002E99F8
    .long D_002E99F8
.globl D_002E9A08
D_002E9A08:
    .long D_002E9A00
    .long D_002E9A00
.globl D_002E9A10
D_002E9A10:
    .long D_002E9A08
    .long D_002E9A08
.globl D_002E9A18
D_002E9A18:
    .long D_002E9A10
    .long D_002E9A10
.globl D_002E9A20
D_002E9A20:
    .long D_002E9A18
    .long D_002E9A18
.globl D_002E9A28
D_002E9A28:
    .long D_002E9A20
    .long D_002E9A20
.globl D_002E9A30
D_002E9A30:
    .long D_002E9A28
    .long D_002E9A28
.globl D_002E9A38
D_002E9A38:
    .long D_002E9A30
    .long D_002E9A30
.globl D_002E9A40
D_002E9A40:
    .long D_002E9A38
    .long D_002E9A38
.globl D_002E9A48
D_002E9A48:
    .long D_002E9A40
    .long D_002E9A40
.globl D_002E9A50
D_002E9A50:
    .long D_002E9A48
    .long D_002E9A48
.globl D_002E9A58
D_002E9A58:
    .long D_002E9A50
    .long D_002E9A50
.globl D_002E9A60
D_002E9A60:
    .long D_002E9A58
    .long D_002E9A58
.globl D_002E9A68
D_002E9A68:
    .long D_002E9A60
    .long D_002E9A60
.globl D_002E9A70
D_002E9A70:
    .long D_002E9A68
    .long D_002E9A68
.globl D_002E9A78
D_002E9A78:
    .long D_002E9A70
    .long D_002E9A70
.globl D_002E9A80
D_002E9A80:
    .long D_002E9A78
    .long D_002E9A78
.globl D_002E9A88
D_002E9A88:
    .long D_002E9A80
    .long D_002E9A80
.globl D_002E9A90
D_002E9A90:
    .long D_002E9A88
    .long D_002E9A88
.globl D_002E9A98
D_002E9A98:
    .long D_002E9A90
    .long D_002E9A90
.globl D_002E9AA0
D_002E9AA0:
    .long D_002E9A98
    .long D_002E9A98
.globl D_002E9AA8
D_002E9AA8:
    .long D_002E9AA0
    .long D_002E9AA0
.globl D_002E9AB0
D_002E9AB0:
    .long D_002E9AA8
    .long D_002E9AA8
.globl D_002E9AB8
D_002E9AB8:
    .long D_002E9AB0
    .long D_002E9AB0
.globl D_002E9AC0
D_002E9AC0:
    .long D_002E9AB8
    .long D_002E9AB8
.globl D_002E9AC8
D_002E9AC8:
    .long D_002E9AC0
    .long D_002E9AC0
.globl D_002E9AD0
D_002E9AD0:
    .long D_002E9AC8
    .long D_002E9AC8
.globl D_002E9AD8
D_002E9AD8:
    .long D_002E9AD0
    .long D_002E9AD0
.globl D_002E9AE0
D_002E9AE0:
    .long D_002E9AD8
    .long D_002E9AD8
.globl D_002E9AE8
D_002E9AE8:
    .long D_002E9AE0
    .long D_002E9AE0
.globl D_002E9AF0
D_002E9AF0:
    .long D_002E9AE8
    .long D_002E9AE8
.globl D_002E9AF8
D_002E9AF8:
    .long D_002E9AF0
    .long D_002E9AF0
.globl D_002E9B00
D_002E9B00:
    .long D_002E9AF8
    .long D_002E9AF8
.globl D_002E9B08
D_002E9B08:
    .long D_002E9B00
    .long D_002E9B00
.globl D_002E9B10
D_002E9B10:
    .long D_002E9B08
    .long D_002E9B08
.globl D_002E9B18
D_002E9B18:
    .long D_002E9B10
    .long D_002E9B10
.globl D_002E9B20
D_002E9B20:
    .long D_002E9B18
    .long D_002E9B18
.globl D_002E9B28
D_002E9B28:
    .long D_002E9B20
    .long D_002E9B20
.globl D_002E9B30
D_002E9B30:
    .long D_002E9B28
    .long D_002E9B28
.globl D_002E9B38
D_002E9B38:
    .long D_002E9B30
    .long D_002E9B30
.globl D_002E9B40
D_002E9B40:
    .long D_002E9B38
    .long D_002E9B38
.globl D_002E9B48
D_002E9B48:
    .long D_002E9B40
    .long D_002E9B40
.globl D_002E9B50
D_002E9B50:
    .long D_002E9B48
    .long D_002E9B48
.globl D_002E9B58
D_002E9B58:
    .long D_002E9B50
    .long D_002E9B50
.globl D_002E9B60
D_002E9B60:
    .long D_002E9B58
    .long D_002E9B58
.globl D_002E9B68
D_002E9B68:
    .long D_002E9B60
    .long D_002E9B60
.globl D_002E9B70
D_002E9B70:
    .long D_002E9B68
    .long D_002E9B68
.globl D_002E9B78
D_002E9B78:
    .long D_002E9B70
    .long D_002E9B70
.globl D_002E9B80
D_002E9B80:
    .long D_002E9B78
    .long D_002E9B78
.globl D_002E9B88
D_002E9B88:
    .long D_002E9B80
    .long D_002E9B80
.globl D_002E9B90
D_002E9B90:
    .long D_002E9B88
    .long D_002E9B88
.globl D_002E9B98
D_002E9B98:
    .long D_002E9B90
    .long D_002E9B90
.globl D_002E9BA0
D_002E9BA0:
    .long D_002E9B98
    .long D_002E9B98
.globl D_002E9BA8
D_002E9BA8:
    .long D_002E9BA0
    .long D_002E9BA0
.globl D_002E9BB0
D_002E9BB0:
    .long D_002E9BA8
    .long D_002E9BA8
.globl D_002E9BB8
D_002E9BB8:
    .long D_002E9BB0
    .long D_002E9BB0
.globl D_002E9BC0
D_002E9BC0:
    .long D_002E9BB8
    .long D_002E9BB8
.globl D_002E9BC8
D_002E9BC8:
    .long D_002E9BC0
    .long D_002E9BC0
.globl D_002E9BD0
D_002E9BD0:
    .long D_002E9BC8
    .long D_002E9BC8
.globl D_002E9BD8
D_002E9BD8:
    .long D_002E9BD0
    .long D_002E9BD0
.globl D_002E9BE0
D_002E9BE0:
    .long D_002E9BD8
    .long D_002E9BD8
.globl D_002E9BE8
D_002E9BE8:
    .long D_002E9BE0
    .long D_002E9BE0
.globl D_002E9BF0
D_002E9BF0:
    .long D_002E9BE8
    .long D_002E9BE8
.globl D_002E9BF8
D_002E9BF8:
    .long D_002E9BF0
    .long D_002E9BF0
.globl D_002E9C00
D_002E9C00:
    .long D_002E9BF8
    .long D_002E9BF8
.globl D_002E9C08
D_002E9C08:
    .long D_002E9C00
    .long D_002E9C00
    .long D_002E9C08
    .long D_002E9C08
.globl D_002E9C18
D_002E9C18:
    .space 8
.globl D_002E9C20
D_002E9C20:
    .space 8
.globl D_002E9C28
D_002E9C28:
    .space 8
.globl D_002E9C30
D_002E9C30:
    .space 8
.globl D_002E9C38
D_002E9C38:
    .space 8
.globl D_002E9C40
D_002E9C40:
    .space 40
.globl D_002E9C68
D_002E9C68:
    .space 4
.globl D_002E9C6C
D_002E9C6C:
    .space 4
.globl D_002E9C70
D_002E9C70:
    .space 16
.globl D_002E9C80
D_002E9C80:
    .space 4
.globl D_002E9C84
D_002E9C84:
    .space 4
.globl D_002E9C88
D_002E9C88:
    .long D_3BE71C
.globl D_002E9C8C
D_002E9C8C:
    .space 4
.globl D_002E9C90
D_002E9C90:
    .space 4
.globl D_002E9C94
D_002E9C94:
    .space 4
.globl D_002E9C98
D_002E9C98:
    .space 8
.globl D_002E9CA0
D_002E9CA0:
    .space 128
.globl D_002E9D20
D_002E9D20:
    .space 4
.globl D_002E9D24
D_002E9D24:
    .space 4
.globl D_002E9D28
D_002E9D28:
    .space 4
.globl D_002E9D2C
D_002E9D2C:
    .space 4
.globl D_002E9D30
D_002E9D30:
    .space 4
.globl D_002E9D34
D_002E9D34:
    .space 4
.globl D_002E9D38
D_002E9D38:
    .long D_002FBAA8
.globl D_002E9D3C
D_002E9D3C:
    .space 4
.globl D_002E9D40
D_002E9D40:
    .space 4
.globl D_002E9D44
D_002E9D44:
    .long D_002FBCC0
.globl D_002E9D48
D_002E9D48:
    .space 8
.globl D_002E9D50
D_002E9D50:
    .space 4
    .long func_002BA308
    .space 4
    .long func_002BA2D0
.globl D_002E9D60
D_002E9D60:
    .space 4
.globl D_002E9D64
D_002E9D64:
    .space 4
.globl D_002E9D68
D_002E9D68:
    .space 1960
.globl D_002EA510
D_002EA510:
    .space 4
    .long func_002BA518
    .space 16
.globl D_002EA528
D_002EA528:
    .space 4
    .long func_002BA688
.globl D_002EA530
D_002EA530:
    .space 1856
.globl D_002EAC70
D_002EAC70:
    .space 40
.globl D_002EAC98
D_002EAC98:
    .space 4
    .long func_002BA938
    .space 56
.globl D_002EACD8
D_002EACD8:
    .space 8
.globl D_002EACE0
D_002EACE0:
    .space 16
.globl D_002EACF0
D_002EACF0:
    .space 8
.globl D_002EACF8
D_002EACF8:
    .space 4
.globl D_002EACFC
D_002EACFC:
    .space 4
.globl D_002EAD00
D_002EAD00:
    .space 816
.globl D_002EB030
D_002EB030:
    .space 8
.globl D_002EB038
D_002EB038:
    .space 64
.globl D_002EB078
D_002EB078:
    .space 8
.globl D_002EB080
D_002EB080:
    .space 4
    .long func_002BC748
    .space 56
.globl D_002EB0C0
D_002EB0C0:
    .space 208
.globl D_002EB190
D_002EB190:
    .space 20
    .long func_00100000
    .space 264
.globl D_002EB2B0
D_002EB2B0:
    .space 128
.globl D_002EB330
D_002EB330:
    .space 16
    .long D_002EB0C0
    .long D_002EB190
    .long D_002EB2B0
