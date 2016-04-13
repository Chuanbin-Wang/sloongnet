-- When message is recved, the fm will call this function.
require_ex('ex');

local main_Req = {};

main_Req.ReloadScript = function( u, req, res )
	ReloadScript();
	return 0;
end

main_Req.SqlTest = function( u, req, res )
	local cmd = req['cmd'] or '';
	showLog("run sql cmd:" .. cmd);
	local res = querySql(cmd);
	showLog(res);
	return 0,res;
end

main_Req.TextTest = function( u, req, res )
        res['TestText'] = getEngineVer()  .. ' -- Sloong Network Engine -- Copyright 2015 Sloong.com. All Rights Reserved';
        return 0
end

-- �ϴ��ļ�����
-- �ͻ���׼��Ҫ�ϴ����ļ���Ϣ,����style �� �ļ���md5,�Լ���չ��
-- ����˼��md5��Ϣ,�����ݼ����,�����Ƿ���Ҫ�ϴ�.�������ϴ���ֱ���봫�������ļ���¼
-- ����Ҫ�ϴ�,�򹹽�һ��uuid, ��·����Ϊuploadurl/user/uuid+��չ���ĸ�ʽ����.
-- �ͻ��˸��ݷ���,����Ҫ�ϴ����ļ�����ָ��Ŀ¼.
-- �ͻ��˷���UploadEnd��Ϣ,����������ΪĿ��·��
-- 
-- 
-- ����˰�����/��/��/uuid�Ľṹ���洢�ļ�
-- get the total for the file need upload
-- then check the all file md5, if file is have one server, 
-- then gen the new guid and create the folder with the guid name.
-- return the path with guid.
-- then client upload the file to the folder, 
function main_Req.UploadStart(u, req, res)
	res['ftpuser']=Get('FTP','User','');
	res['ftppwd']=Get('FTP','Password','');
	showLog(res['filename'])
	res['filename']=req['filename'];
	
	-- get guid from c++
	--GetGUID()
	local guid = GenUUID();
	-- Return a floder path.
	local path = Get('FTP','UploadUrl','') .. '/' .. guid;
	res['filepath']=path;
	res['UploadURL'] = path;
	return 0;
end

function main_Req.UploadEnd( u, req, res )
	local path = req['UploadURL'];
	local newPath = Get('UploadFolder');
	MoveFile(newPath,path);
end

g_all_request_processer = 
{
	['Reload'] = main_Req.ReloadScript,
	['GetText'] = main_Req.TextTest,
	['RunSql'] = main_Req.SqlTest,
	['UploadStart'] = main_Req.UploadStart,
	['UploadEnd'] = main_Req.UploadEnd,
}
AddModule(g_ex_function);
