ScriptName TR_TraderRader_F4SE native hidden

Function otherChest(actor[] a) Global
	Actor aa = a[0] as Actor
	ObjectReference tRef = aa
	if (tRef as AspirationalAddItemScript)
		(tRef as AspirationalAddItemScript).pOtherContainer.RemoveAllItems(tRef)
	elseif (tRef as AspirationalAddItemScript02)
		(tRef as AspirationalAddItemScript02).pOtherContainer.RemoveAllItems(tRef)
	Endif
EndFunction
