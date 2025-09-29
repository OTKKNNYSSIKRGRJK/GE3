struct ObjStruct {
	uint Index;
	int Val;
};

static const uint objSize = 4 + 4;

struct GlobalVariables {
	uint Offset_EntryID;
	uint Mask_ReverseDir;
};

StructuredBuffer<GlobalVariables> GlobalVar : register(t0);

RWByteAddressBuffer rwBuffer : register(u0);

#define ThreadGroupCountX 1
#define ThreadGroupCountY 1
#define NumThreadsX 512
#define NumThreadsY 1

uint TID(uint3 dtid_) {
	return
		dtid_.x +
		dtid_.y * ThreadGroupCountX * NumThreadsX +
		dtid_.z * ThreadGroupCountX * NumThreadsX * ThreadGroupCountY * NumThreadsY;
}

ObjStruct BufEntry(uint tid_) {
	uint startLocation = tid_ * objSize;
	
	ObjStruct ret;
	ret.Index = rwBuffer.Load(startLocation);
	ret.Val = asint(rwBuffer.Load(startLocation + 4));
	
	return ret;
}

void StoreData(uint tid_, ObjStruct obj_) {
	uint startLocation = tid_ * objSize;
	
	rwBuffer.Store(startLocation, obj_.Index);
	rwBuffer.Store(startLocation + 4, asuint(obj_.Val));
}

[numthreads(NumThreadsX, NumThreadsY, 1)]
void main(uint3 dtid_ : SV_DispatchThreadID, uint gIdx_ : SV_GroupIndex) {
	uint tid = TID(dtid_);
	uint mask = tid & (GlobalVar[0].Offset_EntryID - 1);
	uint entryID = (tid << 1) - mask;
	uint dir = !!(entryID & GlobalVar[0].Mask_ReverseDir);
	
	ObjStruct entry0 = BufEntry(entryID);
	ObjStruct entry1 = BufEntry(entryID + GlobalVar[0].Offset_EntryID);
	uint cmpResult = !!(entry0.Val > entry1.Val) ^ dir;
	
	StoreData(entryID + GlobalVar[0].Offset_EntryID * (cmpResult ^ 1), entry0);
	StoreData(entryID + GlobalVar[0].Offset_EntryID * cmpResult, entry1);
	
	//rwBuffer.Store(tid * 40, gtid_.x);
	//rwBuffer.Store(tid * 40 + 4, gtid_.y);
	//rwBuffer.Store(tid * 40 + 8, gtid_.z);
	//rwBuffer.Store(tid * 40 + 12, gid_.x);
	//rwBuffer.Store(tid * 40 + 16, gid_.y);
	//rwBuffer.Store(tid * 40 + 20, gid_.z);
	//rwBuffer.Store(tid * 40 + 24, dtid_.x);
	//rwBuffer.Store(tid * 40 + 28, dtid_.y);
	//rwBuffer.Store(tid * 40 + 32, dtid_.z);
	//rwBuffer.Store(tid * 40 + 36, gidx_);
}