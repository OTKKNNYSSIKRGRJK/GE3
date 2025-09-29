RWTexture2D<float4> Output : register(u0);

#define NUM_THREAD_X 256U

static const float Inv_W = 1.0f / 256.0f;
static const float Inv_H = 1.0f / 256.0f;

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

[numthreads(NUM_THREAD_X, 1U, 1U)]
void main(uint3 dtid_ : SV_DispatchThreadID) {
	Output[dtid_.xy] = float4(dtid_.x * Inv_W, dtid_.x * Inv_H, 0.0f, 1.0f);
}