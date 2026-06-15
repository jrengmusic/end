void main()
{
    vec4 col;
    mainImage (col, gl_FragCoord.xy);
    gl_FragColor = col;
}
