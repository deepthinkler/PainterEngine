#include "PX_MemoryPool.h"

#ifdef PX_DEBUG_MODE

#include "stdio.h"
static px_int DEBUG_i;
static px_int DEBUG_assert;
px_void MP_UnreleaseInfo(px_memorypool *mp)
{
	px_int i;
	for (i=0;i<sizeof(mp->DEBUG_allocdata)/sizeof(mp->DEBUG_allocdata[0]);i++)
	{
		if(mp->DEBUG_allocdata[i].addr) 
			printf("Warning:Unreleased memory in MID %d\n",mp->DEBUG_allocdata[i].addr);

	}
}

#endif


typedef struct _MemoryNode
{
	px_void *StartAddr;
	px_void *EndAddr;
}MemoryNode;




 MemoryNode * PX_MemoryPool_GetFreeTableAddr(px_memorypool *MP)
{
	return (MemoryNode *)((px_uchar *)MP->EndAddr-(sizeof(MemoryNode)*MP->FreeTableCount)+1);
}

 MemoryNode *PX_MemoryPool_GetFreeTable(px_memorypool *MP,px_uint Index)
{
	Index++;
	return (MemoryNode *)((px_uchar *)MP->EndAddr-(sizeof(MemoryNode)*Index)+1);
}

MemoryNode *PX_AllocFromFree(px_memorypool *MP,px_uint Size)
{
	MemoryNode *Node;
	if (Size+2*sizeof(MemoryNode)>MP->FreeSize)
	{
		return 0;
	}
	Node=(MemoryNode *)MP->AllocAddr;
	(*Node).StartAddr=(px_uchar *)MP->AllocAddr+sizeof(MemoryNode);
	(*Node).EndAddr=(px_uchar *)Node->StartAddr+Size-1;

	
	MP->FreeSize-=(Size+2*sizeof(MemoryNode));
	MP->AllocAddr=(px_char *)MP->AllocAddr+Size+sizeof(MemoryNode);

	return Node; 
}




px_void PX_MemoryPoolRemoveFreeNode(px_memorypool *MP,px_uint Index)
{
	px_uint i;
	MemoryNode *pFreeNode=PX_MemoryPool_GetFreeTable(MP,Index);

	
	for (i=Index;i<MP->FreeTableCount;i++)
	{
		(*pFreeNode)=*(pFreeNode-1);
		pFreeNode--;
	}

	MP->FreeTableCount--;
	if (MP->FreeTableCount==0)
	{
		MP->MaxMemoryfragSize=0;
	}

}

MemoryNode *PX_AllocFreeMemoryNode(px_memorypool *MP)
{
	MP->FreeTableCount++;
	return PX_MemoryPool_GetFreeTable(MP,MP->FreeTableCount-1);
}

px_void PX_UpdateMaxFreqSize(px_memorypool *MP)
{
	MemoryNode *itNode;
	px_uint i,Size;

	MP->MaxMemoryfragSize=0;

	for(i=0;i<MP->FreeTableCount;i++)
	{
		itNode=PX_MemoryPool_GetFreeTable(MP,i);
		if ((Size=(px_uint)((px_char *)itNode->EndAddr-(px_char *)itNode->StartAddr+1)>MP->MaxMemoryfragSize))
		{
			MP->MaxMemoryfragSize=Size;
		}
	}
}

MemoryNode *PX_AllocFromFreq(px_memorypool *MP,px_uint Size)
{
	px_uint i,fSize;
	MemoryNode *itNode,*allocNode;

	Size+=sizeof(MemoryNode);

	if (MP->MaxMemoryfragSize>=Size)
	{

		for(i=0;i<MP->FreeTableCount;i++)
		{
			itNode=PX_MemoryPool_GetFreeTable(MP,i);
			fSize=(px_uint)((px_char *)itNode->EndAddr-(px_char *)itNode->StartAddr+1);

			if (Size<=fSize&&(Size+sizeof(MemoryNode)>fSize))
			{
				allocNode=(MemoryNode *)itNode->StartAddr;


				allocNode->StartAddr=(px_char *)itNode->StartAddr+sizeof(MemoryNode);
				allocNode->EndAddr=itNode->EndAddr; 


				PX_MemoryPoolRemoveFreeNode(MP,i);
				PX_UpdateMaxFreqSize(MP);
				return allocNode;
			}
			else 
			{
				if(Size<fSize)
				{
					if (MP->FreeSize<sizeof(MemoryNode))
					{
						return 0;
					}
					allocNode=(MemoryNode *)itNode->StartAddr;
					allocNode->StartAddr=(px_char *)itNode->StartAddr+sizeof(MemoryNode);
					allocNode->EndAddr=(px_char *)itNode->StartAddr+Size-1;

					itNode->StartAddr=(px_char *)allocNode->EndAddr+1;
					MP->FreeSize-=sizeof(MemoryNode);
					PX_UpdateMaxFreqSize(MP);
					return allocNode;
				}
			}

		}
		return 0;
	}
	else
	{
		return 0;
	}
}





px_memorypool MP_Create( px_void *MemoryAddr,px_uint MemorySize )
{
	px_uint Index=0;
	px_memorypool MP;
	MP.StartAddr=MemoryAddr;
	MP.AllocAddr=MemoryAddr;
	if(MemorySize)
	MP.EndAddr=((px_uchar*)MemoryAddr)+MemorySize-1;
	else
	MP.EndAddr=MP.StartAddr;

	MP.Size=MemorySize;
	MP.FreeSize=MemorySize;
	MP.FreeTableCount=0;
	MP.MaxMemoryfragSize=0;
	MP.nodeCount=0;
	MP.ErrorCall_Ptr=PX_NULL;
	px_memset(MemoryAddr,0,MemorySize);

#ifdef PX_DEBUG_MODE

	for (DEBUG_i=0;DEBUG_i<sizeof(MP.DEBUG_allocdata)/sizeof(MP.DEBUG_allocdata[0]);DEBUG_i++)
	{
		MP.DEBUG_allocdata[DEBUG_i].addr=PX_NULL;
		MP.DEBUG_allocdata[DEBUG_i].startAddr=PX_NULL;
		MP.DEBUG_allocdata[DEBUG_i].endAddr=PX_NULL;
	}
#endif
	return MP;

}



px_void * MP_Malloc(px_memorypool *MP, px_uint Size )
{
	MemoryNode *MemNode;
#ifdef PX_DEBUG_MODE
	MP_Append_data *pAppend;
	MemoryNode *itNode;

	for (DEBUG_i=0;DEBUG_i<sizeof(MP->DEBUG_allocdata)/sizeof(MP->DEBUG_allocdata[0]);DEBUG_i++)
	{
		if (MP->DEBUG_allocdata[DEBUG_i].addr!=PX_NULL)
		{
			MP_Append_data *pAppend;
			pAppend=(MP_Append_data *)((px_uchar *)MP->DEBUG_allocdata[DEBUG_i].endAddr-sizeof(MP_Append_data)+1);
			itNode=(MemoryNode *)((px_uchar *)MP->DEBUG_allocdata[DEBUG_i].addr-sizeof(MemoryNode));
			if(MP->DEBUG_allocdata[DEBUG_i].startAddr!=itNode->StartAddr) 
			{
					PX_ASSERT();
					return PX_NULL;
			}
			if(MP->DEBUG_allocdata[DEBUG_i].endAddr!=itNode->EndAddr) 
			{
					PX_ASSERT();
					return PX_NULL;
			}
			if(pAppend->append!=MP_APPENDDATA_MAGIC)
			{
				PX_ASSERT();
				return PX_NULL;
			}
		}
	}

#endif
	if (Size==0)
	{
		return PX_NULL;
	}
#ifdef PX_DEBUG_MODE
	Size+=sizeof(MP_Append_data);
#endif
	if (Size%__PX_MEMORYPOOL_ALIGN_BYTES)
	{
		Size=(Size/__PX_MEMORYPOOL_ALIGN_BYTES+1)*__PX_MEMORYPOOL_ALIGN_BYTES;
	}
	//Allocate from freq

	MemNode=PX_AllocFromFreq(MP,Size);
	if (MemNode!=0)
	{
	#ifdef PX_DEBUG_MODE
		for (DEBUG_i=0;DEBUG_i<sizeof(MP->DEBUG_allocdata)/sizeof(MP->DEBUG_allocdata[0]);DEBUG_i++)
		{
			if(MP->DEBUG_allocdata[DEBUG_i].addr==0)
			{
				MP->DEBUG_allocdata[DEBUG_i].addr=MemNode->StartAddr;
				MP->DEBUG_allocdata[DEBUG_i].startAddr=MemNode->StartAddr;
				MP->DEBUG_allocdata[DEBUG_i].endAddr=MemNode->EndAddr;
				break;
			}
		}
		pAppend=(MP_Append_data *)((px_uchar *)MemNode->EndAddr-sizeof(MP_Append_data)+1);
		pAppend->append=MP_APPENDDATA_MAGIC;
	#endif
		MP->nodeCount++;
		return MemNode->StartAddr;
	}

	//Allocate from free

	MemNode=PX_AllocFromFree(MP,Size);
	if (MemNode!=0)
	{
		#ifdef PX_DEBUG_MODE
		for (DEBUG_i=0;DEBUG_i<sizeof(MP->DEBUG_allocdata)/sizeof(MP->DEBUG_allocdata[0]);DEBUG_i++)
		{
			if(MP->DEBUG_allocdata[DEBUG_i].addr==0)
			{
				MP->DEBUG_allocdata[DEBUG_i].addr=MemNode->StartAddr;
				MP->DEBUG_allocdata[DEBUG_i].startAddr=MemNode->StartAddr;
				MP->DEBUG_allocdata[DEBUG_i].endAddr=MemNode->EndAddr;
				break;
			}
		}
		pAppend=(MP_Append_data *)((px_uchar *)MemNode->EndAddr-sizeof(MP_Append_data)+1);
		pAppend->append=MP_APPENDDATA_MAGIC;
		#endif
		MP->nodeCount++;
		return MemNode->StartAddr;
	}

	if(MP->ErrorCall_Ptr==PX_NULL)
	PX_ERROR("MemoryPool Out Of Memory!");
	else
	MP->ErrorCall_Ptr(PX_MEMORYPOOL_ERROR_OUTOFMEMORY);

	return PX_NULL;

}



px_void MP_Free(px_memorypool *MP, px_void *pAddress )
{
	px_uint i,sIndex;
    MemoryNode *itNode;
	MemoryNode FreeNode;
	px_uchar *pcTempStart,*pcTempEnd;
	px_uchar bExist;
	px_void *TempPointer;
	MemoryNode *TempNode;
	MP->nodeCount--;
	bExist=0;
	//Get Memory node information
	TempPointer=(px_uchar *)pAddress-sizeof(MemoryNode);
	TempNode=(MemoryNode *)TempPointer;
	FreeNode.StartAddr=TempNode->StartAddr;
	FreeNode.EndAddr=TempNode->EndAddr;


#ifdef PX_DEBUG_MODE
	if(FreeNode.StartAddr!=pAddress) PX_ASSERT();
	
	for (DEBUG_i=0;DEBUG_i<sizeof(MP->DEBUG_allocdata)/sizeof(MP->DEBUG_allocdata[0]);DEBUG_i++)
	{
		if(MP->DEBUG_allocdata[DEBUG_i].addr==pAddress)
		{
			MP_Append_data *pAppend;
			pAppend=(MP_Append_data *)((px_uchar *)MP->DEBUG_allocdata[DEBUG_i].endAddr-sizeof(MP_Append_data)+1);
			if(MP->DEBUG_allocdata[DEBUG_i].startAddr!=FreeNode.StartAddr) PX_ASSERT();
			if(MP->DEBUG_allocdata[DEBUG_i].endAddr!=FreeNode.EndAddr) PX_ASSERT();
			if(pAppend->append!=MP_APPENDDATA_MAGIC)PX_ASSERT();
			MP->DEBUG_allocdata[DEBUG_i].addr=PX_NULL;
			MP->DEBUG_allocdata[DEBUG_i].startAddr=PX_NULL;
			MP->DEBUG_allocdata[DEBUG_i].endAddr=PX_NULL;
			break;
		}
	}


	if(DEBUG_i==sizeof(MP->DEBUG_allocdata)/sizeof(MP->DEBUG_allocdata[0]))
	{
		if(MP->ErrorCall_Ptr==PX_NULL)
			PX_LOG("Invalid address free");
		else
			MP->ErrorCall_Ptr(PX_MEMORYPOOL_ERROR_INVALID_ADDRESS);

		PX_ASSERT();
		goto _END;
	}



#endif

	//px_memset(FreeNode.StartAddr,0xff,(px_int)FreeNode.EndAddr-(px_int)FreeNode.StartAddr+1);
	//Reset address
	FreeNode.StartAddr=(px_uchar *)FreeNode.StartAddr-sizeof(MemoryNode);

	//If this memory node is last
	if ((px_char *)FreeNode.EndAddr+1==(px_char *)MP->AllocAddr)
	{
		for(i=0;i<MP->FreeTableCount;i++)
		{
			itNode=PX_MemoryPool_GetFreeTable(MP,i);
			if ((px_char *)itNode->EndAddr+1==(px_char *)FreeNode.StartAddr)
			{
				MP->AllocAddr=itNode->StartAddr;
				MP->FreeSize+=(px_uint32)((px_char *)FreeNode.EndAddr-(px_char *)FreeNode.StartAddr+sizeof(MemoryNode)+1);
				MP->FreeSize+=(px_uint32)((px_char *)itNode->EndAddr-(px_char *)itNode->StartAddr+1+sizeof(MemoryNode));
				PX_MemoryPoolRemoveFreeNode(MP,i);
				goto _END;

			}
		}

		//just reset allocAddr to release this memory node
		MP->AllocAddr=(px_char *)FreeNode.StartAddr;
		MP->FreeSize+=(px_uint32)((px_char *)FreeNode.EndAddr-(px_char *)FreeNode.StartAddr+sizeof(MemoryNode)+1);

		goto _END;
	}



	//Search memory node which can be combine
	sIndex=0xffffffff;
	for(i=0;i<MP->FreeTableCount;i++)
	{
		itNode=PX_MemoryPool_GetFreeTable(MP,i);
		pcTempStart=(px_uchar *)itNode->StartAddr;
		pcTempEnd=(px_uchar *)itNode->EndAddr;


		if (pcTempStart==(px_uchar *)FreeNode.EndAddr+1)
		{
			if(sIndex==0xffffffff)
			{
			sIndex=i;
			//Refresh this node
			itNode->StartAddr=FreeNode.StartAddr;
			FreeNode=*itNode;
			MP->FreeSize+=sizeof(MemoryNode);
			}
			else
			{
				MP->FreeSize+=sizeof(MemoryNode);
				itNode->StartAddr=FreeNode.StartAddr;
				PX_MemoryPoolRemoveFreeNode(MP,sIndex);
			}
			bExist=1;
		}


		if (pcTempEnd+1==(px_uchar *)FreeNode.StartAddr)
		{
			if(sIndex==0xffffffff)
			{
				sIndex=i;
				//Refresh this node
				itNode->EndAddr=FreeNode.EndAddr;
				FreeNode=*itNode;
				MP->FreeSize+=sizeof(MemoryNode);
			}
			else
			{
				itNode->EndAddr=FreeNode.EndAddr;
				MP->FreeSize+=sizeof(MemoryNode);
				PX_MemoryPoolRemoveFreeNode(MP,sIndex);
				
			}
			bExist=1;
		}
	}
	if(bExist==0)
	{
#ifdef PX_DEBUG_MODE
		for(i=0;i<MP->FreeTableCount;i++)
		{
			itNode=PX_MemoryPool_GetFreeTable(MP,i);
			if (FreeNode.StartAddr>=itNode->StartAddr&&FreeNode.StartAddr<=itNode->EndAddr)
			{
				//alloc error
				PX_ASSERT();
				goto _END;
			}
			if (FreeNode.EndAddr>=itNode->StartAddr&&FreeNode.EndAddr<=itNode->EndAddr)
			{
				PX_ASSERT();
				goto _END;
			}
		}
#endif
		*PX_AllocFreeMemoryNode(MP)=FreeNode;
	}


	PX_UpdateMaxFreqSize(MP);
_END:
	#ifdef PX_DEBUG_MODE
	for (DEBUG_i=0;DEBUG_i<sizeof(MP->DEBUG_allocdata)/sizeof(MP->DEBUG_allocdata[0]);DEBUG_i++)
	{
		if(MP->DEBUG_allocdata[DEBUG_i].addr!=PX_NULL)
		{
			for(i=0;i<MP->FreeTableCount;i++)
			{
				itNode=PX_MemoryPool_GetFreeTable(MP,i);
				if (MP->DEBUG_allocdata[DEBUG_i].startAddr>=itNode->StartAddr&&MP->DEBUG_allocdata[DEBUG_i].startAddr<=itNode->EndAddr)
				{
					//error free
					PX_ASSERT();
				}
				if (MP->DEBUG_allocdata[DEBUG_i].endAddr>=itNode->StartAddr&&MP->DEBUG_allocdata[DEBUG_i].endAddr<=itNode->EndAddr)
				{
					PX_ASSERT();
				}
			}
		}
	}
#endif
	return;
}

px_void MP_Release(px_memorypool *Node)
{
	//free(G_MemoryPool.StartAddr);
}

px_void MP_ErrorCatch(px_memorypool *Pool,PX_MP_ErrorCall ErrorCall)
{
	Pool->ErrorCall_Ptr=ErrorCall;
}

px_uint MP_Size(px_memorypool *Pool,px_void *pAddress)
{
	px_void *TempPointer;
	MemoryNode *TempNode;
	//Get Memory node information
	if (pAddress==PX_NULL)
	{
		PX_ASSERT();
		return 0;
	}
	TempPointer=(px_uchar *)pAddress-sizeof(MemoryNode);
	TempNode=(MemoryNode *)TempPointer;
#ifdef PX_DEBUG_MODE
return (px_uint)(((px_char *)(TempNode->EndAddr)-(px_char *)(TempNode->StartAddr))+1-sizeof(MP_Append_data));
#else
	return ((px_char *)(TempNode->EndAddr)-(px_char *)(TempNode->StartAddr))+1;
#endif
	
}

px_void MP_Reset(px_memorypool *Pool)
{
	Pool->AllocAddr=Pool->StartAddr;
	Pool->EndAddr=((px_uchar*)Pool->StartAddr)+Pool->Size-1;
	Pool->FreeSize=Pool->Size;
	Pool->FreeTableCount=0;
	Pool->MaxMemoryfragSize=0;
	Pool->nodeCount=0;
}

