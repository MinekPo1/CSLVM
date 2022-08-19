
// using an enum allows the usage of a switch statement
enum Instruction {
	I_unknown,
	I_ldi,
	I_loadAtVar,
	I_storeAtVar,
	I_jts,
	I_ret,
	I_addWithVar,
	I_subWithVar,
	I_mulWithVar,
	I_divWithVar,
	I_bitwiseLsfWithVar,
	I_bitwiseRsfWithVar,
	I_bitwiseAndWithVar,
	I_bitwiseOrWithVar,
	I_modWithVar,
	I_print,
	I_println,
	I_jmp,
	I_jt,
	I_jf,
	I_boolAndWithVar,
	I_boolOrWithVar,
	I_boolEqualWithVar,
	I_largerThanOrEqualWithVar,
	I_smallerThanOrEqualWithVar,
	I_boolNotEqualWithVar,
	I_smallerThanWithVar,
	I_largerThanWithVar,
	I_putPixel,
	I_putLine,
	I_putRect,
	I_setColor,
	I_clg,
	I_done,
	I_malloc,
	I_round,
	I_floor,
	I_ceil,
	I_cos,
	I_sin,
	I_sqrt,
	I_atan2,
	I_mouseDown,
	I_mouseX,
	I_mouseY,
	I_sleep,
	I_drawText,
	I_loadAtVarWithOffset,
	I_storeAtVarWithOffset,
	I_isKeyPressed,
	I_createColor,
	I_charAt,
	I_sizeOf,
	I_contains,
	I_join,
	I_setStrokeWidth,
	I_inc,
	I_dec,
	I_graphicsFlip,
	I_newLine,
	I_ask,
	I_setCloudVar,
	I_getCloudVar,
	I_indexOfChar,
	I_goto,
	I_imalloc,
	I_getValueAtPointer,
	I_setValueAtPointer,
	I_runtimeMillis,
	I_free,
	I_getVarAddress,
	I_setVarAddress,
	I_copyVar,
	I_incA,
	I_decA,
	I_arrayBoundsCheck,
	I_getValueAtPointerOfA,
	I_stackPushA,
	I_stackPopA,
	I_stackPush,
	I_stackPop,
	I_stackPeekA,
	I_stackPeek,
	I_stackInc,
	I_stackDec,
	I_stackAdd,
	I_stackSub,
	I_stackMul,
	I_stackDiv,
	I_stackBitwiseLsf,
	I_stackBitwiseRsf,
	I_stackBitwiseAnd,
	I_stackBitwiseOr,
	I_stackMod,
	I_stackBoolAnd,
	I_stackBoolOr,
	I_stackBoolEqual,
	I_stackLargerThanOrEqual,
	I_stackSmallerThanOrEqual,
	I_stackNotEqual,
	I_stackSmallerThan,
	I_stackLargerThan,
	I_conditionalValueSet,
};
