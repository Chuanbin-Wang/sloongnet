function require_ex( _mname )
  print( string.format("require_ex = %s", _mname) )
  if package.loaded[_mname] then
    print( string.format("require_ex module[%s] reload", _mname))
  end
  package.loaded[_mname] = nil
  require( _mname )
end

Init = function( path )
    package.path = path .. 'scripts/?.lua';
    print('load comm lua file.:' .. tostring(require('comm')))
    print('load proc lua file:' .. tostring(require('epollproc')))

end

OnError = function( msg )
    print('error proc:' .. msg);
end
