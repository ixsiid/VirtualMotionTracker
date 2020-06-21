#include "CommunicationManager.h"
namespace VMTDriver {
	//����M�X���b�h
	std::mutex CommunicationWorkerMutex;
	std::deque<string> CommunicationWorkerReadQue;
	std::deque<string> CommunicationWorkerWriteQue;
	bool CommunicationWorkerExit = false;
	std::thread *CommunicationWorkerThread;
	void CommunicationWorker()
	{
		while (!CommunicationWorkerExit)
		{
			//�N���e�B�J���Z�N�V����
			{
				std::lock_guard<std::mutex> lock(CommunicationWorkerMutex); 

				//��M�f�[�^����������Ђ�����ǂݍ���
				string r = CommunicationManager::GetInstance()->GetSM()->readM2D();
				//��M�f�[�^���L��ꍇ(�����~�b�^�[�ȉ��̏ꍇ)
				if (r != "" && CommunicationWorkerReadQue.size() < 1024) {
					CommunicationWorkerReadQue.push_back(r);
					//��ɂȂ�܂ő҂����ԍŒZ�œǂݍ���
					continue;
				}
				//���M�f�[�^����������Ђ����珑������
				if (!CommunicationWorkerWriteQue.empty())
				{
					if (CommunicationManager::GetInstance()->GetSM()->writeD2M(CommunicationWorkerWriteQue.front()) == true)
					{
						//������������������đ�����
						CommunicationWorkerWriteQue.pop_front();
						continue;
					}
					//�������ݎ��s���͎��̎����܂ő҂�(���肪�N�����Ă��Ȃ����A���肪�����ς������ς�)
				}
			}
			//�m���N���e�B�J���Z�N�V����
			Sleep(4); //240fps
		}
	}

	string CommunicationRead() {
		string r = "";
		//�N���e�B�J���Z�N�V����
		{
			std::lock_guard<std::mutex> lock(CommunicationWorkerMutex);
			if (!CommunicationWorkerReadQue.empty())
			{
				//��M�L���[������o��
				r = CommunicationWorkerReadQue.front();
				CommunicationWorkerReadQue.pop_front();
			}
		}
		return r;
	}
	void CommunicationWrite(string s) {
		std::lock_guard<std::mutex> lock(CommunicationWorkerMutex);
		if (CommunicationWorkerWriteQue.size() < 1024) { //1024���ȏ�͎̂Ă�(�ُ펞�A�ʐM�s�ǎ�)
			CommunicationWorkerWriteQue.push_back(s);
		}
	}

	CommunicationManager* CommunicationManager::GetInstance()
	{
		static CommunicationManager cm;
		return &cm;
	}
	SharedMemory::SharedMemory* CommunicationManager::GetSM()
	{
		return m_sm;
	}
	void CommunicationManager::Open()
	{
		if (m_opened) {
			return;
		}
		m_sm = new SharedMemory::SharedMemory();
		if (!m_sm->open()) {
			//�I�[�v���Ɏ��s
			delete m_sm;
			m_sm = nullptr;
			return;
		}
		
		CommunicationWorkerThread = new std::thread(CommunicationWorker);
		m_opened = true;
	}
	void CommunicationManager::Close()
	{
		CommunicationWorkerExit = true;
		CommunicationWorkerThread->join();
		m_sm->close();
		delete m_sm;
		m_sm = nullptr;
		m_opened = false;
	}
	void CommunicationManager::Process(ServerTrackedDeviceProvider* server)
	{
		//�ʐM�̏������ł��Ă��Ȃ�
		if (m_sm == nullptr) {
			return;
		}
		try {
			//��M(�o�b�t�@�����܂��Ă���ꍇ�����ɏ�������)
			do {
				string r = CommunicationRead();
				if (r.empty()) {
					break;
				}
				//Log::printf("CommunicationWorkerReadQue:%d", CommunicationWorkerReadQue.size());

				json j = json::parse(r);
				string type = j["type"];
				if (type == "Pos") {
					string j2s = j["json"];
					json j2 = json::parse(j2s);


					DriverPose_t pose{ 0 };
					pose.deviceIsConnected = true;
					pose.poseIsValid = true;
					pose.result = TrackingResult_Running_OK;

					pose.qRotation = VMTDriver::HmdQuaternion_Identity;
					pose.qWorldFromDriverRotation = VMTDriver::HmdQuaternion_Identity;
					pose.qDriverFromHeadRotation = VMTDriver::HmdQuaternion_Identity;

					pose.vecPosition[0] = j2["x"];
					pose.vecPosition[1] = j2["y"];
					pose.vecPosition[2] = j2["z"];
					pose.qRotation.x = j2["qx"];
					pose.qRotation.y = j2["qy"];
					pose.qRotation.z = j2["qz"];
					pose.qRotation.w = j2["qw"];
					server->GetDevices()[0].SetPose(pose);
				}
				printf("%s\n", type.c_str());
			} while (true);

			//���M
			/*
			json jw;
			jw["type"] = "Hello";
			jw["json"] = json{ {"msg","Hello from cpp"} }.dump();
			CommunicationWrite(jw.dump());
			*/
		}
		catch (...) {
			m_sm->logError("Exception in CommunicationManager::Process");
		}
	}
}