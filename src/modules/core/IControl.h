/*** 
 * @Author: Chuanbin Wang - wcb@sloong.com
 * @Date: 2018-02-28 10:55:37
 * @LastEditTime: 2020-08-12 15:44:38
 * @LastEditors: Chuanbin Wang
 * @FilePath: /engine/src/modules/core/IControl.h
 * @Copyright 2015-2020 Sloong.com. All Rights Reserved
 * @Description: 
 */
/*** 
 * @......................................&&.........................
 * @....................................&&&..........................
 * @.................................&&&&............................
 * @...............................&&&&..............................
 * @.............................&&&&&&..............................
 * @...........................&&&&&&....&&&..&&&&&&&&&&&&&&&........
 * @..................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&..............
 * @................&...&&&&&&&&&&&&&&&&&&&&&&&&&&&&.................
 * @.......................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.........
 * @...................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&...............
 * @..................&&&   &&&&&&&&&&&&&&&&&&&&&&&&&&&&&............
 * @...............&&&&&@  &&&&&&&&&&..&&&&&&&&&&&&&&&&&&&...........
 * @..............&&&&&&&&&&&&&&&.&&....&&&&&&&&&&&&&..&&&&&.........
 * @..........&&&&&&&&&&&&&&&&&&...&.....&&&&&&&&&&&&&...&&&&........
 * @........&&&&&&&&&&&&&&&&&&&.........&&&&&&&&&&&&&&&....&&&.......
 * @.......&&&&&&&&.....................&&&&&&&&&&&&&&&&.....&&......
 * @........&&&&&.....................&&&&&&&&&&&&&&&&&&.............
 * @..........&...................&&&&&&&&&&&&&&&&&&&&&&&............
 * @................&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&............
 * @..................&&&&&&&&&&&&&&&&&&&&&&&&&&&&..&&&&&............
 * @..............&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&....&&&&&............
 * @...........&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&......&&&&............
 * @.........&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.........&&&&............
 * @.......&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&...........&&&&............
 * @......&&&&&&&&&&&&&&&&&&&...&&&&&&...............&&&.............
 * @.....&&&&&&&&&&&&&&&&............................&&..............
 * @....&&&&&&&&&&&&&&&.................&&...........................
 * @...&&&&&&&&&&&&&&&.....................&&&&......................
 * @...&&&&&&&&&&.&&&........................&&&&&...................
 * @..&&&&&&&&&&&..&&..........................&&&&&&&...............
 * @..&&&&&&&&&&&&...&............&&&.....&&&&...&&&&&&&.............
 * @..&&&&&&&&&&&&&.................&&&.....&&&&&&&&&&&&&&...........
 * @..&&&&&&&&&&&&&&&&..............&&&&&&&&&&&&&&&&&&&&&&&&.........
 * @..&&.&&&&&&&&&&&&&&&&&.........&&&&&&&&&&&&&&&&&&&&&&&&&&&.......
 * @...&&..&&&&&&&&&&&&.........&&&&&&&&&&&&&&&&...&&&&&&&&&&&&......
 * @....&..&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&...........&&&&&&&&.....
 * @.......&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&..............&&&&&&&....
 * @.......&&&&&.&&&&&&&&&&&&&&&&&&..&&&&&&&&...&..........&&&&&&....
 * @........&&&.....&&&&&&&&&&&&&.....&&&&&&&&&&...........&..&&&&...
 * @.......&&&........&&&.&&&&&&&&&.....&&&&&.................&&&&...
 * @.......&&&...............&&&&&&&.......&&&&&&&&............&&&...
 * @........&&...................&&&&&&.........................&&&..
 * @.........&.....................&&&&........................&&....
 * @...............................&&&.......................&&......
 * @................................&&......................&&.......
 * @.................................&&..............................
 * @..................................&..............................
 */

/*
 * @Author: WCB
 * @Date: 1970-01-01 08:00:00
 * @LastEditors: WCB
 * @LastEditTime: 2020-04-16 15:41:07
 * @Description: file content
 */
#pragma once

#include "IEvent.h"
namespace Sloong
{
	typedef std::function<void(SharedEvent)> MsgHandlerFunc;
	class IControl
	{
	public:
		// Data
		// Add DATE_TIME to dictory.
		virtual void Add(uint64_t, void *) = 0;
		virtual void *Get(uint64_t) = 0;
		virtual void Remove(uint64_t) = 0;
		virtual void AddTempString(const string &, const string &) = 0;
		virtual void AddTempObject(const string &, const void *, int) = 0;
		virtual void AddTempBytes(const string &, unique_ptr<char[]> &, int) = 0;
		virtual void AddTempSharedPtr(const string &, shared_ptr<void>) = 0;

		// Get temp string, the param is key.
		// If key not exist, return empty string
		// If exist, return the value and remove from dictory.
		virtual string GetTempString(const string &, bool = true) = 0;
		virtual void *GetTempObject(const string &, int *, bool = true) = 0;
		virtual unique_ptr<char[]> GetTempBytes(const string &, int *) = 0;

		// The return value is shared_ptr<void>, so must use the static_pointer_case to convert the type.
		// and if want convert shared_ptr from base class to chiled class, should use dynamic_pointer_case , not static_pointer_case.
		virtual shared_ptr<void> GetTempSharedPtr(const string &, bool = true) = 0;

		virtual bool ExistTempBytes(const string &) = 0;
		virtual bool ExistTempObject(const string &) = 0;
		virtual bool ExistTempString(const string &) = 0;
		virtual bool ExistTempSharedPtr(const string &) = 0;
		// Message
		virtual void SendMessage(int32_t) = 0;
		virtual void SendMessage(SharedEvent) = 0;
		virtual void CallMessage(SharedEvent) = 0;
		virtual void RegisterEventHandler(int32_t, MsgHandlerFunc) = 0;
	};
} // namespace Sloong
