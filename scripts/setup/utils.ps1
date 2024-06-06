function GetPD {
    param (
        [Parameter(Position=0)]
        [System.Object]
        $object,
        [Parameter(Position=1)]
        [System.Object]
        $default
    )
    return ($null -ne $object) ? $object : $default
}
