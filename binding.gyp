{
    'targets' : [
	{
	    'target_name' : 'curllib.node',
	    'type' : 'shared_library',
	    'sources' : [
		'curllib.cc'
	    ],
	    'libraries' : [
		'-lcurl'
	    ]
	}
    ]
}
