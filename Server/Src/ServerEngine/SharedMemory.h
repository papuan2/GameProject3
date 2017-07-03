#ifndef __SHARE_MEMORY_H__
#define __SHARE_MEMORY_H__
#include "..\ServerData\serverStruct.h"

#define BLOCK_CHECK_CODE	0x5A
#define  SafeDelete(a) if(a!=NULL){delete a; a=NULL;}

/**共享内存的状态
*/
enum SharedMemoryState
{
	SMS_NONE,  ///未使用，空闲状态
	SMS_USE,    //已经使用了，数据库服务器可以读取修改写入数据库
	SMS_LOCK,///锁住状态，逻辑服务器正在写入
	SMS_RELEASE,///逻辑服务器已经释放了。数据库服务器写入修改后可以置为SMS_NONE状态
	SMS_DELETE,///删除标志
};

///所有放到sharedMemory里的元素都必须是从ShareObject派生的
struct ShareObject
{
	ShareObject();

	///开始修改，标记为被占用
	void lock();

	///判断是否被占有用
	BOOL islock()const;

	///标记为个改完成。
	void unlock();

	void useit();

	///标记为已经释放了
	void release();

	void destroy();

	BOOL isDestroy() const;

	BOOL isRelease() const;

	time_t getLastMotifyTime();

	///是否在使用
	BOOL isUse() const;

	void reset();

	UINT32					   m_dwCheckCode;
	SharedMemoryState          m_State;
	time_t                     m_updatetime;	///最后一次修改时间
};

///记录每个T块的状态
struct _SMBlock
{
	UINT32			m_dwIndex;      //数据当前编号
	BOOL			m_bUse;         //是否在使用true是正在使用，false是没有使用
	BOOL			m_bNewBlock;	///是否是刚刚新创建的区块
	time_t			m_beforeTime;   //DS服务器更新完成后回写的信息时间。
	time_t          m_afterTime;    
	_SMBlock()
	{
		m_dwIndex = 0;
		m_bUse = false;
		m_beforeTime = 0;
		m_afterTime = 0;
		m_bNewBlock = false;
	}
};

///共享内存页结构
struct shareMemoryPage
{
	char*        m_pdata;///指定共享内存地址
	_SMBlock*    m_pBlock;///数据块的头位置
	HANDLE       m_shm;///
};

class SharedMemoryBase
{
public:
	SharedMemoryBase(const std::string& name, UINT32 rawblockSize, UINT32 count, BOOL noCreate = false);

	SharedMemoryBase(UINT32 rawblockSize, char* pdata, INT32 len);

	virtual ~SharedMemoryBase();
protected:

	typedef std::vector<shareMemoryPage> ShareMemoryPageMapping;
	///共享内存页映射.
	ShareMemoryPageMapping m_ShareMemoryPageMapping;

	UINT32		 m_countperPage;///页面容纳T类型数量
	UINT32		 m_pageCount;///页数量
	UINT32       m_count;///T类型的总个数,T类型必须是定长的。
	UINT32       m_space;///每个元素的宽度
	UINT32		m_rawblockSize;
	std::string m_modulename;
	BOOL		isempty;

	///所有数据头的集合
	typedef  std::map<INT32, _SMBlock*>    mapSMBlock;
	mapSMBlock                     m_mapSMBlock;///所有数据块头信息


	typedef std::map<void*, _SMBlock*>  mapUsedSMBlock;
	mapUsedSMBlock                 m_mapUsedSMBlock;	///所有使用了块的数据信息


	typedef std::map<INT32, _SMBlock*> mapFreeSMBlock;
	mapFreeSMBlock                 m_mapFreeSMBlock;///所有空闲的块信息
private:

	///创建一个新页
	BOOL newPage();


	/**
	* @brief		初始化数据区域
	* @details		数据清0，并设置保护区域
	* @param[in]	rPage : 共享内存页
	* @return		void
	* @remarks
	*/
	void initpage(shareMemoryPage& rPage);


public:

	///数据库服务器不需要初始化map,逻辑服务器才需要,所以分开
	void InitToMap();


	/**是否是首创共享内存*/
	BOOL isFirstCreated();


	/**从共享内存里恢复其他页*/
	void importOtherPage();

	/**获取数量*/
	const UINT32 getCount()const;

	/**获取还有多少块空闲内存
	*/
	UINT32 getFreeCount()const;

	///获取已经使用了多少块
	UINT32 getUseCount()const;

	/**通过id获取原始内存中的描述块指针
	*/
	virtual _SMBlock* getSMBbyRawIndex(INT32 index);

	/**通过id获取原始内存中的描述块指针
	*/
	virtual ShareObject*  getObjectByRawindex(UINT32 index);


	const UINT32 getRawMemoryBlockSize();

	const INT32 getBlockSize() { return m_rawblockSize; }

	/*处理已用区块中被数据库服务器释放的区块*/
	void processCleanDirtyData();

	/*从空闲内存中分配一个块,如果没有了返回空
	@param isNewBlock 为true时会在保存期调用saveobject 的Create虚函数
	*/
	virtual ShareObject* newOjbect(BOOL isNewBlock = false);

	/**释放一块已经不再使用的内存
	*/
	virtual BOOL destoryObject(ShareObject* pobject);
};

template<typename T>
class SharedMemory : public SharedMemoryBase
{
public:
	SharedMemory(const std::string& name, UINT32 count, BOOL noCreate = false)
		: SharedMemoryBase(name, sizeof(T), count, noCreate)
	{

	}

	SharedMemory(const std::string& name, char* pdata, INT32 len)
		: SharedMemoryBase(sizeof(T), pdata, len)
	{

	}

	T*  getObjectByRawindex(UINT32 index)
	{
		return static_cast<T*>(SharedMemoryBase::getObjectByRawindex(index));
	}

	T*  getObjectByindex(UINT32 index)
	{
		return static_cast<T*>(SharedMemoryBase::getObjectByindex(index));
	}

	T* newOjbect(BOOL isNewBlock = false)
	{
		T* pTmp = static_cast<T*>(SharedMemoryBase::newOjbect(isNewBlock));
		if (pTmp == NULL)
		{
			return NULL;
		}

		new(pTmp)(T);
		return pTmp;
	}

	_SMBlock* getSMBbyRawIndex(INT32 index)
	{
		return SharedMemoryBase::getSMBbyRawIndex(index);
	}

	BOOL destoryObject(T* pobject)
	{
		return SharedMemoryBase::destoryObject(pobject);
	}
};



template <typename T> class DataWriter
{
public:
	DataWriter(const std::string& modulename)
	{
		m_MemoryPool = NULL;
		m_moduleName = modulename;
		m_count = 1024;
	}
	~DataWriter()
	{
		SafeDelete(m_MemoryPool);
	}

	/**数据库修改*/
	BOOL saveModifyToDB(IDataBase* pdb)
	{
		///共享内存不存在直接返回
		if (m_MemoryPool == NULL)
		{
			m_MemoryPool = new SharedMemory<T>(m_moduleName, m_count, true);
		}
		if (m_MemoryPool == NULL)
		{
			return false;
		}

		if (m_MemoryPool->isFirstCreated())
		{
			///共享内存还没创建
			SafeDelete(m_MemoryPool);
			return false;
		}

		INT32 newtimes = 0, writetimes = 0, deletetimes = 0, releasetime = 0;
		BOOL hasOprate = false; //是否有操作
		///获取所有修改过的数据,getRawMemoryBlockSize会重新计算所有共享块，
		UINT32 temblockSize = m_MemoryPool->getRawMemoryBlockSize();
		for (UINT32 r = 0; r < temblockSize; r++)
		{
			_SMBlock* pBlock = m_MemoryPool->getSMBbyRawIndex(r);
			if (pBlock == NULL)
			{
				continue;
			}
			if (pBlock->m_bUse == false)
			{
				continue;
			}
			T* pdata = m_MemoryPool->getObjectByRawindex(r);
			if (pdata == NULL)
			{
				continue;
			}
			if (pdata->m_dwCheckCode != BLOCK_CHECK_CODE)
			{
				continue;
			}
			if (!pdata->isUse())
			{
				continue;
			}
			///正在修改数据,跳过
			if (pdata->islock())
			{
				continue;
			}
			///优先回调删除
			if (pdata->isDestroy())
			{
				///创建一个副本发送异步执行,提高运行效率
				pdata->Delete(pdb);
				m_MemoryPool->destoryObject(pdata);
				hasOprate = true;
				deletetimes++;
				continue;
			}
			///其次回调新建
			if (pBlock->m_bNewBlock)
			{
				///创建一个副本发送异步执行,提高运行效率
				pdata->Save(pdb);
				pBlock->m_bNewBlock = false;
				pBlock->m_afterTime = time(NULL);
				hasOprate = true;
				newtimes++;
				continue;
			}

			time_t lastMotifyTime;
			time_t beforeTime, afterTime;
			lastMotifyTime = pdata->getLastMotifyTime();
			beforeTime = pBlock->m_beforeTime;
			afterTime = pBlock->m_afterTime;
			BOOL needsave = false;
			///保存完毕的时间大于保存前的时间,那么上一次保存成功的
			if (afterTime >= beforeTime)
			{
				if (lastMotifyTime < beforeTime)
				{
					needsave = true;
				}
			}
			else
			{
				needsave = true;///上一次保存失败,立即保存
			}

			if (needsave)
			{
				///创建一个副本发送异步执行,提高运行效率
				pBlock->m_beforeTime = time(NULL);
				pdata->Save(pdb);
				hasOprate = true;
				writetimes++;
				pBlock->m_afterTime =time(NULL);
				continue;
			}
	
		
			if (pdata->isRelease())
			{
				///释放的时候执行一次保存...如果上次没有保存成功或者，释放前修改了就再保存一次
				if ((lastMotifyTime > 0) && (afterTime < beforeTime || lastMotifyTime > beforeTime)) ///change by dsq
				{
					pBlock->m_beforeTime = time(NULL);/// add by dsq
					pdata->Save(pdb);
					hasOprate = true;
					writetimes++;
					continue; 
				}
				m_MemoryPool->destoryObject(pdata);
				releasetime++;
			}
			
		}


		//xLogMessager::getSingleton().logMessage(std::string("sync db module:") + m_moduleName +
		//                                        std::string(" finished.write:") + Helper::IntToString(writetimes) +
		//                                        std::string(" .new:") + Helper::IntToString(newtimes) +
		//                                        std::string(" .desdroy:") + Helper::IntToString(deletetimes) +
		//                                        std::string(".release:") + Helper::IntToString(releasetime),
		//                                        Log_ErrorLevel
		//                                       );
		return hasOprate;
	}
private:
	SharedMemory<T>*       m_MemoryPool;///模块内存池
	UINT32		           m_count;///共享内存大小
	std::string m_moduleName;
};

#endif