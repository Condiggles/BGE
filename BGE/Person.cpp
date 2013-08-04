#include "Person.h"
#include "Game.h"
#include <sstream>
#include "Box.h"
#include "Cylinder.h"

using namespace BGE;
using namespace std;

void CALLBACK BGE::StatusProc( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName, void * pUserData)
{      
	Person * person = (Person *) pUserData;
	if ( SUCCEEDED( hrStatus ) )      
	{   person->CreateFirstConnected();       
		person->connected = true;      
	}      
	else      
	{          
		person->connected = false;       
	}
}
Person::Person(void):GameComponent()
{
	connected = false;
}


Person::~Person(void)
{
}

bool Person::Initialise()
{
	CreateFirstConnected();
	NuiSetDeviceStatusCallback(&StatusProc, this);
	return GameComponent::Initialise();
}

HRESULT BGE::Person::CreateFirstConnected()
{
	INuiSensor * pNuiSensor;

	int iSensorCount = 0;
	HRESULT hr = NuiGetSensorCount(&iSensorCount);
	if (FAILED(hr))
	{
		return hr;
	}

	// Look at each Kinect sensor
	for (int i = 0; i < iSensorCount; ++i)
	{
		// Create the sensor so we can check status, if we can't create it, move on to the next
		hr = NuiCreateSensorByIndex(i, &pNuiSensor);
		if (FAILED(hr))
		{
			continue;
		}

		// Get the status of the sensor, and if connected, then we can initialize it
		hr = pNuiSensor->NuiStatus();
		if (S_OK == hr)
		{
			m_pNuiSensor = pNuiSensor;
			break;
		}

		// This sensor wasn't OK, so release it since we're not using it
		pNuiSensor->Release();
		return hr;
	}

	if (NULL != m_pNuiSensor)
	{
		// Initialize the Kinect and specify that we'll be using skeleton
		hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON); 
		if (SUCCEEDED(hr))
		{
			// Create an event that will be signaled when skeleton data is available
			m_hNextSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
			// Open a skeleton stream to receive skeleton data
			hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, 0); 
		}
	}

	if (NULL == m_pNuiSensor || FAILED(hr))
	{
		SetStatusMessage("No ready Kinect found!");
		return E_FAIL;
	}
	connected = true;
	SetStatusMessage("Kinect Connected");
	return hr;
}

void Person::UpdateSkeleton(const NUI_SKELETON_DATA & skeleton)
{
	footHeight = glm::min<float>(skeleton.SkeletonPositions[NUI_SKELETON_POSITION_FOOT_RIGHT].y, skeleton.SkeletonPositions[NUI_SKELETON_POSITION_FOOT_LEFT].y);

	UpdateBone(skeleton, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT);

	UpdateBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);

	UpdateBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);

	UpdateBone(skeleton, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);

	UpdateBone(skeleton, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT);
	UpdateBone(skeleton, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);
}

glm::vec3 NUIToGLVector( Vector4 v )
{
	return glm::vec3(v.x, v.y, v.z);
}

void Person::UpdateBone(
	const NUI_SKELETON_DATA & skeleton,
	NUI_SKELETON_POSITION_INDEX jointFrom,
	NUI_SKELETON_POSITION_INDEX jointTo)
{
	NUI_SKELETON_POSITION_TRACKING_STATE jointFromState = skeleton.eSkeletonPositionTrackingState[jointFrom];
	NUI_SKELETON_POSITION_TRACKING_STATE jointToState = skeleton.eSkeletonPositionTrackingState[jointTo];

	
	if (jointFromState == NUI_SKELETON_POSITION_NOT_TRACKED || jointToState == NUI_SKELETON_POSITION_NOT_TRACKED)
	{
		return; // nothing to draw, one of the joints is not tracked
	}

	glm::vec3 start = NUIToGLVector(skeleton.SkeletonPositions[jointFrom]);
	glm::vec3 end = NUIToGLVector(skeleton.SkeletonPositions[jointTo]);

	float scale = 30.0f;
	start *= scale;
	end *= scale;
	start.y += 10;
	end.y += 10;
	start.z *= -1;
	end.z *= -1;

	glm::vec3 boneVector = end - start;
	float boneLength = glm::length(boneVector);
	glm::vec3 centrePos = start + ((boneVector) / 2.0f);
	boneVector = glm::normalize(boneVector);
	glm::vec3 axis = glm::cross(GameComponent::basisUp, boneVector);
	axis = glm::normalize(axis);
	float theta = (float) glm::acos<float>(glm::dot<float>(GameComponent::basisUp, boneVector));
	glm::quat q = glm::angleAxis(glm::degrees(theta), axis);

	stringstream ss;
	ss << jointFrom << "," << jointTo;
	string boneKey = ss.str();

	map<string, GameComponent *>::iterator it = boneComponents.find(boneKey);
	GameComponent * cyl = NULL;
	if (it == boneComponents.end())
	{
		cyl = new Cylinder(0.5f, boneLength);
		cyl->Initialise();
		boneComponents[boneKey] = cyl;
		cyl->attachedToParent = false;
		AddChild(cyl);
	}
	else
	{
		cyl = it->second;
	}
	cyl->position = centrePos;
	cyl->orientation = q;
}

void Person::SkeletonFrameReady( NUI_SKELETON_FRAME* pSkeletonFrame )
{
	for (int i = 0; i < NUI_SKELETON_COUNT; i++)
	{
		const NUI_SKELETON_DATA & skeleton = pSkeletonFrame->SkeletonData[i];

		switch (skeleton.eTrackingState)
		{
		case NUI_SKELETON_TRACKED:
			UpdateSkeleton(skeleton);
			break;
		}
	}
}

void Person::Update(float timeDelta)
{
	if (connected)
	{
		SetStatusMessage("Kinect is tracking");
		// Wait for 0ms, just quickly test if it is time to process a skeleton
		if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0) )
		{
			NUI_SKELETON_FRAME skeletonFrame = {0};

			// Get the skeleton frame that is ready
			if (SUCCEEDED(m_pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame)))
			{
				// Process the skeleton frame
				SkeletonFrameReady(&skeletonFrame);
			}
		}
	}
	else
	{
		SetStatusMessage("Kinect is not tracking");
	}

	GameComponent::Update(timeDelta);
}

void Person::SetStatusMessage( std::string message )
{
	Game::Instance()->PrintText(message);
}